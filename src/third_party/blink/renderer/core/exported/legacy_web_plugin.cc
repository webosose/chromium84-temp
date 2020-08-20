// Copyright 2018-2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "third_party/blink/renderer/core/exported/legacy_web_plugin.h"

#include <dlfcn.h>
#include <string.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_bindings.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_impl.h"  //_NPN_RetainObject
#include "third_party/blink/renderer/bindings/core/npapi/v8_np_object.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/legacy_web_plugin_info.h"
#include "third_party/blink/renderer/core/exported/legacy_web_plugin_list.h"
#include "third_party/blink/renderer/core/exported/plugin_stream_url.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

#define PLUGINS_BASE_PATH "/usr/lib/BrowserPlugins/"

// Declarations for stub implementations of deprecated functions, which are no
// longer listed in npapi.h.
extern "C" {
void* NPN_GetJavaEnv();
void* NPN_GetJavaPeer(NPP);
}

namespace blink {

class LegacyWebPlugin::LoaderClient : public WebAssociatedURLLoaderClient {
 public:
  LoaderClient(PluginStreamUrl*,
               WebAssociatedURLLoader*,
               unsigned long resource_id);
  ~LoaderClient() override;
  bool WillFollowRedirect(const WebURL& new_url,
                          const WebURLResponse& redirect_response) override {
    return true;
  }
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const WebURLResponse&) override;
  void DidDownloadData(uint64_t data_length) override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidReceiveCachedMetadata(const char* data, int data_length) override;
  void DidFinishLoading() override;
  void DidFail(const WebURLError&) override;

  PluginStreamUrl* stream_;
  std::unique_ptr<WebAssociatedURLLoader> loader_;
  unsigned long resource_id;
};

LegacyWebPlugin::LoaderClient::LoaderClient(PluginStreamUrl* stream,
                                            WebAssociatedURLLoader* loader,
                                            unsigned long resource_id)
    : stream_(stream), loader_(loader), resource_id(resource_id) {
  DCHECK(loader_ != nullptr);
  DCHECK(stream_ != nullptr);
}

LegacyWebPlugin::LoaderClient::~LoaderClient() {}

void LegacyWebPlugin::LoaderClient::DidSendData(
    uint64_t bytes_sent,
    uint64_t total_bytes_to_be_sent) {
  // Ignore
}

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  explicit HeaderFlattener(std::string* buf) : buf_(buf) {}

  void VisitHeader(const WebString& name, const WebString& value) override {
    // TODO(darin): Should we really exclude headers with an empty value?
    if (!name.IsEmpty() && !value.IsEmpty()) {
      buf_->append(name.Utf8());
      buf_->append(": ");
      buf_->append(value.Utf8());
      buf_->append("\n");
    }
  }

 private:
  std::string* buf_;
};

std::string GetAllHeaders(const WebURLResponse& response) {
  // TODO(darin): It is possible for httpStatusText to be empty and still have
  // an interesting response, so this check seems wrong.
  std::string result;
  const WebString& status = response.HttpStatusText();
  if (status.IsEmpty())
    return result;

  // TODO(darin): Shouldn't we also report HTTP version numbers?
  result = base::StringPrintf("HTTP %d ", response.HttpStatusCode());
  result.append(status.Utf8());
  result.append("\n");

  HeaderFlattener flattener(&result);
  response.VisitHttpHeaderFields(&flattener);

  return result;
}

struct ResponseInfo {
  KURL url;
  std::string mime_type;
  uint32_t last_modified;
  uint32_t expected_length;
};

void GetResponseInfo(const WebURLResponse& response,
                     ResponseInfo* response_info) {
  response_info->url = response.CurrentRequestUrl();
  response_info->mime_type = response.MimeType().Utf8();

  // Measured in seconds since 12:00 midnight GMT, January 1, 1970.
  response_info->last_modified =
      static_cast<uint32_t>(base::Time::Now().ToDoubleT() * 1000.0);

  // If the length comes in as -1, then it indicates that it was not
  // read off the HTTP headers. We replicate Safari webkit behavior here,
  // which is to set it to 0.
  response_info->expected_length = static_cast<uint32_t>(
      std::max(response.ExpectedContentLength(), (int64_t)0));

  WebString header = WebString::FromUTF8("Content-Encoding");
  WebString content_encoding = response.HttpHeaderField(header);
  if (!content_encoding.IsNull() &&
      !base::EqualsASCII(base::StringPiece16(content_encoding.Utf16()),
                         base::StringPiece(std::string("identity")))) {
    // Don't send the compressed content length to the plugin, which only
    // cares about the decoded length.
    response_info->expected_length = 0;
  }
}

void LegacyWebPlugin::LoaderClient::DidReceiveResponse(
    const WebURLResponse& response) {
  LWP_LOG_MEMBER_FUNCTION;
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  // TODO(jam): THIS LOGIC IS COPIED IN PluginURLFetcher::OnReceivedResponse
  // until kDirectNPAPIRequests is the default and we can remove this old path.
  if (stream_ == nullptr) {
    return;
  }

  ResponseInfo response_info;
  GetResponseInfo(response, &response_info);

  bool request_is_seekable = false;

  // Calling into a plugin could result in reentrancy if the plugin yields
  // control to the OS like entering a modal loop etc. Prevent this by
  // stopping further loading until the plugin notifies us that it is ready to
  // accept data
  loader_->SetDefersLoading(true);

  stream_->DidReceiveResponse(response_info.mime_type, GetAllHeaders(response),
                              response_info.expected_length,
                              response_info.last_modified, request_is_seekable);
}

void LegacyWebPlugin::LoaderClient::DidDownloadData(uint64_t data_length) {
  // Ignore
  LWP_LOG_MEMBER_FUNCTION;
}

void LegacyWebPlugin::LoaderClient::DidReceiveData(const char* data,
                                                   int data_length) {
  LWP_LOG_MEMBER_FUNCTION;
  LWP_LOG("Received " << data_length << " bytes");

  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;

  if (stream_ == nullptr) {
    return;
  }
  loader_->SetDefersLoading(true);
  stream_->DidReceiveData(data, data_length, -1);
  LWP_LOG("Exit DidReceiveData");
}

void LegacyWebPlugin::LoaderClient::DidReceiveCachedMetadata(const char* data,
                                                             int data_length) {
  // Ignore
}

void LegacyWebPlugin::LoaderClient::DidFinishLoading() {
  LWP_LOG_MEMBER_FUNCTION;
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  if (stream_ == nullptr) {
    return;
  }
  PluginStreamUrl* url_stream = stream_;
  stream_ = nullptr;
  url_stream->DidFinishLoading(0);
}

void LegacyWebPlugin::LoaderClient::DidFail(const WebURLError&) {
  LWP_LOG_MEMBER_FUNCTION;
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  if (stream_ == nullptr) {
    return;
  }
  PluginStreamUrl* url_stream = stream_;
  stream_ = nullptr;
  url_stream->DidFail(0);
}

#if defined(USE_NEVA_MEDIA)
void LegacyWebPlugin::DidSuppressMediaPlay(bool suppressed) {
#if defined(OS_WEBOS)
  NPEvent np_event;
  memset(&np_event, 0, sizeof(np_event));
  np_event.eventType = suppressed ? NPWebosPause : NPWebosResume;
  if (plugin_functions_.event != nullptr) {
    plugin_functions_.event(&instance_, &np_event);
  }
#endif
}
#endif

LegacyWebPlugin* LegacyWebPlugin::current_instance_ = nullptr;
LegacyWebPlugin* LegacyWebPlugin::CurrentLegacyWebPlugin() {
  return current_instance_;
}

static LegacyWebPlugin* LegacyWebPluginForInstance(NPP instance) {
  if (instance && instance->ndata)
    return static_cast<LegacyWebPlugin*>(instance->ndata);
  return LegacyWebPlugin::CurrentLegacyWebPlugin();
}

LegacyWebPlugin* LegacyWebPlugin::TryCreateLegacyWebPlugin(
    const String& mime_type,
    blink::WebFrame* frame,
    const blink::WebPluginParams& params) {
  LWP_LOG("TryCreateLegacyWebPlugin: "
          << "mime_type=" << mime_type.Utf8()
          << "; URL=" << params.url.GetString().Utf8())
  const char* libname = nullptr;
  LegacyWebPluginList* plugin_list = LegacyWebPluginList::GetInstance();
  std::string actual_mime_type;
  LegacyWebPluginInfo info;
  if (!plugin_list->GetPluginInfo(params.url, WebString(mime_type).Utf8(), info,
                                  actual_mime_type)) {
    LWP_LOG_ERROR("No plugin found; mime_type="
                  << mime_type.Utf8()
                  << "; URL=" << params.url.GetString().Utf8());
    return nullptr;
  }

  blink::WebPluginParams actual_params = params;
  actual_params.mime_type = WebString::FromUTF8(actual_mime_type);

  LWP_LOG("Found plugin libname=" << info.path.value()
                                  << "; mimeType=" << mime_type.Utf8()
                                  << "; URL=" << params.url.GetString().Utf8())
  libname = info.path.value().c_str();
  return libname ? new LegacyWebPlugin(frame, actual_params, libname) : nullptr;
}

LegacyWebPlugin::LegacyWebPlugin(WebFrame* frame,
                                 const WebPluginParams& params,
                                 const char* library_name)
    : frame_(frame),
      mode_(params.load_manually ? NP_FULL : NP_EMBED),
      plugin_url_(params.url),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      mime_type_(params.mime_type),
      weak_factory_(this) {
  instance_.ndata = this;
  instance_.pdata = nullptr;
  memset(&np_window_, 0, sizeof(np_window_));
  memset(&plugin_functions_, 0, sizeof(plugin_functions_));

  plugin_functions_.size = sizeof(NPPluginFuncs);
  main_thread_this_ = weak_factory_.GetWeakPtr();

  SetParameters(params.attribute_names, params.attribute_values);
  InitializeHostFuncs();

#ifdef OS_WEBOS
  strcpy(library_name_, PLUGINS_BASE_PATH);
#else
  // Test WAM demo on PC/Ubuntu specific
  char path_bin[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path_bin, sizeof(path_bin) - 1);
  if (len != -1) {
    path_bin[len] = '\0';
    base::FilePath file_path(path_bin);
    std::string path_dir = file_path.DirName().AsUTF8Unsafe();
    path_dir += "/";
    strcpy(library_name_, path_dir.c_str());
  } else {
    LWP_LOG_MEMBER_ERROR("readlink failed");
    library_name_[0] = '\0';
  }
#endif

  strcat(library_name_, library_name);
  LWP_LOG("Creating a plugin with lib: " << library_name);
}

void LegacyWebPlugin::FreeStringArray(char** stringArray, int length) {
  if (!stringArray)
    return;

  for (int i = 0; i < length; i++)
    WTF::Partitions::FastFree(stringArray[i]);

  WTF::Partitions::FastFree(stringArray);
}

static char* createUTF8String(const WebString& str) {
  const size_t l = str.Utf8().size();
  char* result =
      reinterpret_cast<char*>(WTF::Partitions::FastMalloc(l + 1, nullptr));

  memcpy(result, str.Utf8().data(), l);
  result[l] = '\0';

  return result;
}

void LegacyWebPlugin::SetParameters(const WebVector<WebString>& paramNames,
                                    const WebVector<WebString>& paramValues) {
  assert(paramNames.size() == paramValues.size());

  unsigned size = paramNames.size();
  unsigned paramCount = 0;

  param_names_ = reinterpret_cast<char**>(
      WTF::Partitions::FastMalloc(sizeof(char*) * size, nullptr));
  param_values_ = reinterpret_cast<char**>(
      WTF::Partitions::FastMalloc(sizeof(char*) * size, nullptr));

  for (unsigned i = 0; i < size; i++) {
    param_names_[paramCount] = createUTF8String(paramNames[i]);
    param_values_[paramCount] = createUTF8String(paramValues[i]);

    paramCount++;
  }

  param_count_ = paramCount;
}

void* NPN_MemAlloc(uint32_t size) {
  return malloc(size);
}

void NPN_MemFree(void* ptr) {
  if (ptr != nullptr && ptr != reinterpret_cast<void*>(-1))
    free(ptr);
}

uint32_t NPN_MemFlush(uint32_t size) {
  // This is not relevant on Windows; MAC specific
  return size;
}

void NPN_ReloadPlugins(NPBool reload_pages) {
  LWP_LOG_FUNCTION;
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* range_list) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

NPError NPN_GetURLNotify(NPP id,
                         const char* url,
                         const char* target,
                         void* notify_data) {
  LWP_LOG_FUNCTION;
  LWP_LOG("url: " << url << ", target: " << (uint64_t)target);
  LegacyWebPlugin* instance = LegacyWebPluginForInstance(id);
  if (instance) {
    return instance->GetURLNotify(url, target, true, notify_data);
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPN_GetURL(NPP id, const char* url, const char* target) {
  LWP_LOG_FUNCTION;
  LWP_LOG("url: " << url << ", target: " << (uint64_t)target);
  LegacyWebPlugin* instance = LegacyWebPluginForInstance(id);
  if (instance) {
    return instance->GetURLNotify(url, target, false, nullptr);
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURLNotify(NPP id,
                          const char* url,
                          const char* target,
                          uint32_t len,
                          const char* buf,
                          NPBool file,
                          void* notify_data) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURL(NPP id,
                    const char* url,
                    const char* target,
                    uint32_t len,
                    const char* buf,
                    NPBool file) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

NPError NPN_NewStream(NPP id,
                      NPMIMEType type,
                      const char* target,
                      NPStream** stream) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

int32_t NPN_Write(NPP id, NPStream* stream, int32_t len, void* buffer) {
  LWP_LOG_FUNCTION;
  return 0;
}

NPError NPN_DestroyStream(NPP id, NPStream* stream, NPReason reason) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

const char* NPN_UserAgent(NPP id) {
  LWP_LOG_FUNCTION;
  LegacyWebPlugin* instance = LegacyWebPluginForInstance(id);
  if (instance) {
    return instance->UserAgent();
  }
  return nullptr;
}

void NPN_Status(NPP id, const char* message) {
  LWP_LOG_FUNCTION;
}

void NPN_InvalidateRect(NPP id, NPRect* invalidRect) {
  LWP_LOG_FUNCTION;
}

void NPN_InvalidateRegion(NPP id, NPRegion invalidRegion) {
  LWP_LOG_FUNCTION;
}

void NPN_ForceRedraw(NPP id) {
  LWP_LOG_FUNCTION;
}

static const char* prettyNameForNPNVariable(NPNVariable variable) {
  switch (variable) {
    case NPNVxDisplay:
      return "NPNVxDisplay";
    case NPNVxtAppContext:
      return "NPNVxtAppContext";
    case NPNVnetscapeWindow:
      return "NPNVnetscapeWindow";
    case NPNVjavascriptEnabledBool:
      return "NPNVjavascriptEnabledBool";
    case NPNVasdEnabledBool:
      return "NPNVasdEnabledBool";
    case NPNVisOfflineBool:
      return "NPNVisOfflineBool";

    case NPNVserviceManager:
      return "NPNVserviceManager (not supported)";
    case NPNVDOMElement:
      return "NPNVDOMElement (not supported)";
    case NPNVDOMWindow:
      return "NPNVDOMWindow (not supported)";
    case NPNVToolkit:
      return "NPNVToolkit (not supported)";
    case NPNVSupportsXEmbedBool:
      return "NPNVSupportsXEmbedBool (not supported)";

    case NPNVWindowNPObject:
      return "NPNVWindowNPObject";
    case NPNVPluginElementNPObject:
      return "NPNVPluginElementNPObject";
    case NPNVSupportsWindowless:
      return "NPNVSupportsWindowless";
    case NPNVprivateModeBool:
      return "NPNVprivateModeBool";

#ifdef XP_MACOSX
    case NPNVpluginDrawingModel:
      return "NPNVpluginDrawingModel";
#ifndef NP_NO_QUICKDRAW
    case NPNVsupportsQuickDrawBool:
      return "NPNVsupportsQuickDrawBool";
#endif
    case NPNVsupportsCoreGraphicsBool:
      return "NPNVsupportsCoreGraphicsBool";
    case NPNVsupportsOpenGLBool:
      return "NPNVsupportsOpenGLBool";
    case NPNVsupportsCoreAnimationBool:
      return "NPNVsupportsCoreAnimationBool";
#ifndef NP_NO_CARBON
    case NPNVsupportsCarbonBool:
      return "NPNVsupportsCarbonBool";
#endif
    case NPNVsupportsCocoaBool:
      return "NPNVsupportsCocoaBool";
#endif

    default:
      return "Unknown variable";
  }
}

static const char* prettyNameForNPPVariable(NPPVariable variable) {
  switch (variable) {
    case NPPVpluginNameString:
      return "NPPVpluginNameString";
    case NPPVpluginDescriptionString:
      return "NPPVpluginDescriptionString";
    case NPPVpluginWindowBool:
      return "NPPVpluginWindowBool";
    case NPPVpluginTransparentBool:
      return "NPPVpluginTransparentBool";

    case NPPVjavaClass:
      return "NPPVjavaClass (not supported)";
    case NPPVpluginWindowSize:
      return "NPPVpluginWindowSize (not supported)";
    case NPPVpluginTimerInterval:
      return "NPPVpluginTimerInterval (not supported)";
    case NPPVpluginScriptableInstance:
      return "NPPVpluginScriptableInstance (not supported)";
    case NPPVpluginScriptableIID:
      return "NPPVpluginScriptableIID (not supported)";
    case NPPVjavascriptPushCallerBool:
      return "NPPVjavascriptPushCallerBool (not supported)";
    case NPPVpluginKeepLibraryInMemory:
      return "NPPVpluginKeepLibraryInMemory (not supported)";
    case NPPVpluginNeedsXEmbed:
      return "NPPVpluginNeedsXEmbed (not supported)";

    case NPPVpluginScriptableNPObject:
      return "NPPVpluginScriptableNPObject";

    case NPPVformValue:
      return "NPPVformValue (not supported)";
    case NPPVpluginUrlRequestsDisplayedBool:
      return "NPPVpluginUrlRequestsDisplayedBool (not supported)";

    case NPPVpluginWantsAllNetworkStreams:
      return "NPPVpluginWantsAllNetworkStreams";
    case NPPVpluginCancelSrcStream:
      return "NPPVpluginCancelSrcStream";

#ifdef XP_MACOSX
    case NPPVpluginDrawingModel:
      return "NPPVpluginDrawingModel";
    case NPPVpluginEventModel:
      return "NPPVpluginEventModel";
    case NPPVpluginCoreAnimationLayer:
      return "NPPVpluginCoreAnimationLayer";
#endif

    default:
      return "Unknown variable";
  }
}

NPError NPN_GetValue(NPP id, NPNVariable variable, void* value) {
  LegacyWebPlugin* instance = LegacyWebPluginForInstance(id);
  if (instance) {
    return instance->GetValue(variable, value);
  }
  return NPERR_GENERIC_ERROR;
}

NPError LegacyWebPlugin::GetValue(NPNVariable variable, void* value) {
  LWP_LOG("GetValue: " << prettyNameForNPNVariable(variable));

  switch (variable) {
    case NPNVWindowNPObject: {
      NPObject* windowScriptObject = frame_->windowObject();

      // Return value is expected to be retained, as described here:
      // <http://www.mozilla.org/projects/plugin/npruntime.html>
      if (windowScriptObject) {
        _NPN_RetainObject(windowScriptObject);
      }

      void** v = (void**)value;
      *v = windowScriptObject;

      return NPERR_NO_ERROR;
    }
    case NPNVPluginElementNPObject: {
      NPObject* pluginScriptObject =
          container_->GetScriptableObjectForElement();

      // Return value is expected to be retained, as described here:
      // <http://www.mozilla.org/projects/plugin/npruntime.html>
      if (pluginScriptObject)
        _NPN_RetainObject(pluginScriptObject);

      void** v = (void**)value;
      *v = pluginScriptObject;

      return NPERR_NO_ERROR;
    }

    default:
      return NPERR_GENERIC_ERROR;
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPN_SetValue(NPP id, NPPVariable variable, void* value) {
  LegacyWebPlugin* instance = LegacyWebPluginForInstance(id);
  if (instance) {
    return instance->SetValue(variable, value);
  }
  return NPERR_GENERIC_ERROR;
}

NPError LegacyWebPlugin::SetValue(NPPVariable variable, void* value) {
  LWP_LOG("SetValue: " << prettyNameForNPPVariable(variable));
  switch (variable) {
    case NPPVpluginWindowBool:
      is_windowed_ = value;
      return NPERR_NO_ERROR;
    case NPPVpluginTransparentBool:
      is_transparent_ = value;
      return NPERR_NO_ERROR;

    default:
      return NPERR_GENERIC_ERROR;
  }
}

NPError LegacyWebPlugin::GetURLNotify(const char* url,
                                      const char* target,
                                      bool need_notify,
                                      void* notify_data) {
  LWP_LOG_MEMBER_FUNCTION;
  WebLocalFrame* frame = nullptr;
  if (!frame_->IsWebLocalFrame()) {
    return NPERR_GENERIC_ERROR;
  }

  frame = static_cast<WebLocalFrame*>(frame_);

  WebURL weburl = frame->GetDocument().CompleteURL(WebString::FromUTF8(url));
  WebURLRequest request(weburl);
  request.SetSiteForCookies(frame->GetDocument().SiteForCookies());
  request.SetHttpMethod(WebString::FromUTF8("GET"));
  request.SetRequestContext(mojom::RequestContextType::PLUGIN);
  request.SetSkipServiceWorker(true);
  request.SetMode(network::mojom::RequestMode::kCors);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kInclude);

  unsigned long resource_id = GetNextResourceId();
  if (!resource_id)
    return NPERR_GENERIC_ERROR;

  WebAssociatedURLLoaderOptions loader_options;
  loader_options.untrusted_http = true;
  WebAssociatedURLLoader* loader =
      frame->CreateAssociatedURLLoader(loader_options);
  PluginStreamUrl* stream =
      new PluginStreamUrl(resource_id, weburl, this, need_notify, notify_data);
  LoaderClient* client = new LoaderClient(stream, loader, resource_id);
  clients_.push_back(std::unique_ptr<LoaderClient>(client));
  open_streams_.push_back(stream);
  client->loader_->LoadAsynchronously(request, client);

  return NPERR_NO_ERROR;
}

void LegacyWebPlugin::OnPluginThreadAsyncCall(void (*func)(void*),
                                              void* user_data) {
  // Do not invoke the callback if NPP_Destroy has already been invoked.
  // TODO: what needs to be checked here?
  // if (webplugin_)
  LWP_LOG_MEMBER_FUNCTION;
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  func(user_data);
  LWP_LOG("OnPluginThreadAsyncCall exit");
}

void LegacyWebPlugin::OnDownloadPluginSrcUrl() {
  GetURLNotify(plugin_url_.GetString().Utf8().c_str(), nullptr, false, nullptr);
}

uint32_t LegacyWebPlugin::ScheduleTimer(uint32_t interval,
                                        NPBool repeat,
                                        void (*func)(NPP id,
                                                     uint32_t timer_id)) {
  // Use next timer id.
  uint32_t timer_id;
  timer_id = next_timer_id_;
  ++next_timer_id_;
  DCHECK(next_timer_id_ != 0);

  // Record timer interval and repeat.
  TimerInfo info;
  info.interval = interval;
  info.repeat = repeat ? true : false;
  timers_[timer_id] = info;

  // Schedule the callback.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LegacyWebPlugin::OnTimerCall, main_thread_this_, func,
                     base::Unretained(GetNPP()), timer_id),
      base::TimeDelta::FromMilliseconds(interval));
  return timer_id;
}

void LegacyWebPlugin::UnscheduleTimer(uint32_t timer_id) {
  // Remove info about the timer.
  TimerMap::iterator it = timers_.find(timer_id);
  if (it != timers_.end())
    timers_.erase(it);
}

class UserAgentHolder {
 public:
  UserAgentHolder(WebFrame* frame) {
    DCHECK(frame != nullptr);
    if (frame == nullptr)
      return;
    if (!frame->IsWebLocalFrame())
      return;
    String user_agent =
        To<WebLocalFrameImpl>(frame)->GetFrame()->Loader().UserAgent();
    if (user_agent.IsNull() || user_agent.IsEmpty())
      return;
    WebString web_user_agent(user_agent);
    user_agent_ = web_user_agent.Utf8();
  }
  const char* UserAgent() { return user_agent_.c_str(); }

 private:
  std::string user_agent_;
};

const char* LegacyWebPlugin::UserAgent() {
  static base::NoDestructor<UserAgentHolder> holder(frame_);
  return (*holder).UserAgent();
}

void LegacyWebPlugin::OnTimerCall(void (*func)(NPP id, uint32_t timer_id),
                                  NPP id,
                                  uint32_t timer_id) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  // Do not invoke callback if the timer has been unscheduled.
  TimerMap::iterator it = timers_.find(timer_id);
  if (it == timers_.end())
    return;

  // Get all information about the timer before invoking the callback. The
  // callback might unschedule the timer.
  TimerInfo info = it->second;

  func(id, timer_id);

  // If the timer was unscheduled by the callback, just free up the timer id.
  if (timers_.find(timer_id) == timers_.end())
    return;

  // Reschedule repeating timers after invoking the callback so callback is not
  // re-entered if it pumps the message loop.
  if (info.repeat) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LegacyWebPlugin::OnTimerCall, main_thread_this_, func,
                       base::Unretained(GetNPP()), timer_id),
        base::TimeDelta::FromMilliseconds(info.interval));
  } else {
    timers_.erase(it);
  }
}

void LegacyWebPlugin::RemoveClient(size_t i) {
  clients_.erase(clients_.begin() + i);
}

unsigned long LegacyWebPlugin::GetNextResourceId() {
  if (!frame_)
    return 0;
  WebView* view = frame_->View();
  if (!view)
    return 0;
  return view->CreateUniqueIdentifierForRequest();
}

void* NPN_GetJavaEnv() {
  LWP_LOG_FUNCTION;
  return nullptr;
}

void* NPN_GetJavaPeer(NPP) {
  LWP_LOG_FUNCTION;
  return nullptr;
}

void NPN_PushPopupsEnabledState(NPP id, NPBool enabled) {
  LWP_LOG_FUNCTION;
}

void NPN_PopPopupsEnabledState(NPP id) {
  LWP_LOG_FUNCTION;
}

void LegacyWebPlugin::PluginThreadAsyncCall(void (*func)(void*),
                                            void* userData) {
  LWP_LOG_FUNCTION;
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LegacyWebPlugin::OnPluginThreadAsyncCall,
                            main_thread_this_, func, userData));
}

NPP LegacyWebPlugin::GetNPP() {
  return &instance_;
}

NPError LegacyWebPlugin::NPP_NewStream(NPMIMEType type,
                                       NPStream* stream,
                                       NPBool seekable,
                                       unsigned short* stype) {
  LWP_LOG_MEMBER_FUNCTION;
  DCHECK(plugin_functions_.newstream != 0);
  if (plugin_functions_.newstream != 0) {
    return plugin_functions_.newstream(&instance_, type, stream, seekable,
                                       stype);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError LegacyWebPlugin::NPP_DestroyStream(NPStream* stream, NPReason reason) {
  LWP_LOG_MEMBER_FUNCTION;
  DCHECK(plugin_functions_.destroystream != 0);

  if (stream == NULL || !IsValidStream(stream) || (stream->ndata == NULL))
    return NPERR_INVALID_INSTANCE_ERROR;

  if (plugin_functions_.destroystream != 0) {
    NPError result =
        plugin_functions_.destroystream(&instance_, stream, reason);
    stream->ndata = NULL;
    return result;
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

int LegacyWebPlugin::NPP_WriteReady(NPStream* stream) {
  LWP_LOG_MEMBER_FUNCTION;
  DCHECK(plugin_functions_.writeready != 0);
  if (plugin_functions_.writeready != 0) {
    return plugin_functions_.writeready(&instance_, stream);
  }
  return 0;
}

int LegacyWebPlugin::NPP_Write(NPStream* stream,
                               int offset,
                               int len,
                               void* buffer) {
  LWP_LOG_MEMBER_FUNCTION;
  DCHECK(plugin_functions_.write != 0);
  if (plugin_functions_.write != 0) {
    return plugin_functions_.write(&instance_, stream, offset, len, buffer);
  }
  return 0;
}

void LegacyWebPlugin::NPP_StreamAsFile(NPStream* stream, const char* name) {
  LWP_LOG_MEMBER_FUNCTION;
  DCHECK(plugin_functions_.asfile != 0);
  if (plugin_functions_.asfile != 0) {
    plugin_functions_.asfile(&instance_, stream, name);
  }
  base::FilePath file_name = base::FilePath::FromUTF8Unsafe(name);
  files_created_.push_back(file_name);
}

void LegacyWebPlugin::NPP_URLNotify(const char* url,
                                    NPReason reason,
                                    void* notifyData) {
  LWP_LOG_MEMBER_FUNCTION;
  DCHECK(plugin_functions_.urlnotify != 0);
  if (plugin_functions_.urlnotify != 0) {
    plugin_functions_.urlnotify(&instance_, url, reason, notifyData);
  }
}

void LegacyWebPlugin::NPP_URLRedirectNotify(const char* url,
                                            int32_t status,
                                            void* notify_data) {
  LWP_LOG_MEMBER_FUNCTION;
  if (plugin_functions_.urlredirectnotify != 0) {
    plugin_functions_.urlredirectnotify(&instance_, url, status, notify_data);
  }
}

bool LegacyWebPlugin::IsValidStream(const NPStream* stream) {
  LWP_LOG_MEMBER_FUNCTION;
  std::vector<scoped_refptr<PluginStreamUrl>>::iterator stream_index;
  for (stream_index = open_streams_.begin();
       stream_index != open_streams_.end(); ++stream_index) {
    if ((*stream_index)->Stream() == stream)
      return true;
  }

  return false;
}

void LegacyWebPlugin::CloseStreams() {
  LWP_LOG_MEMBER_FUNCTION;
  in_close_streams_ = true;
  for (unsigned int index = 0; index < open_streams_.size(); ++index) {
    // Close all streams on the way down.
    open_streams_[index]->Close(NPRES_USER_BREAK);
  }
  open_streams_.clear();
  in_close_streams_ = false;
}

void LegacyWebPlugin::RemoveStream(PluginStreamUrl* stream) {
  LWP_LOG_MEMBER_FUNCTION;
  if (in_close_streams_)
    return;
  std::vector<scoped_refptr<PluginStreamUrl>>::iterator stream_index;
  for (stream_index = open_streams_.begin();
       stream_index != open_streams_.end(); ++stream_index) {
    if (stream_index->get() == stream) {
      LWP_LOG("RemoveStream: Erase stream start");
      // open_streams_.erase(stream_index);
      LWP_LOG("RemoveStream: Erase stream end");
      break;
    }
  }
}

void LegacyWebPlugin::CancelResource(unsigned long resource_id) {
  LWP_LOG_MEMBER_FUNCTION;
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i]->resource_id == resource_id) {
      if (clients_[i]->loader_.get()) {
        clients_[i]->stream_ = nullptr;
        clients_[i]->loader_->SetDefersLoading(false);
        clients_[i]->loader_->Cancel();
        RemoveClient(i);
      }
      return;
    }
  }
}

void LegacyWebPlugin::SetDeferLoading(unsigned long resource_id, bool value) {
  LWP_LOG_MEMBER_FUNCTION;
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i]->resource_id == resource_id) {
      if (clients_[i]->loader_.get()) {
        LWP_LOG("SetDeferLoading=" << value << " resource_id=" << resource_id);
        clients_[i]->loader_->SetDefersLoading(value);
      }
      return;
    }
  }
}

bool LegacyWebPlugin::handles_url_redirects() const {
  return false;
}

void NPN_PluginThreadAsyncCall(NPP id, void (*func)(void*), void* user_data) {
  LegacyWebPlugin* instance = LegacyWebPluginForInstance(id);
  instance->PluginThreadAsyncCall(func, user_data);
}

NPError NPN_GetValueForURL(NPP id,
                           NPNURLVariable variable,
                           const char* url,
                           char** value,
                           uint32_t* len) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

NPError NPN_SetValueForURL(NPP id,
                           NPNURLVariable variable,
                           const char* url,
                           const char* value,
                           uint32_t len) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

NPError NPN_GetAuthenticationInfo(NPP id,
                                  const char* protocol,
                                  const char* host,
                                  int32_t port,
                                  const char* scheme,
                                  const char* realm,
                                  char** username,
                                  uint32_t* ulen,
                                  char** password,
                                  uint32_t* plen) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

uint32_t NPN_ScheduleTimer(NPP id,
                           uint32_t interval,
                           NPBool repeat,
                           void (*func)(NPP id, uint32_t timer_id)) {
  LWP_LOG_FUNCTION;
  return 0;
}

void NPN_UnscheduleTimer(NPP id, uint32_t timer_id) {
  LWP_LOG_FUNCTION;
}

NPError NPN_PopUpContextMenu(NPP id, NPMenu* menu) {
  LWP_LOG_FUNCTION;
  return NPERR_GENERIC_ERROR;
}

NPBool NPN_ConvertPoint(NPP id,
                        double sourceX,
                        double sourceY,
                        NPCoordinateSpace sourceSpace,
                        double* destX,
                        double* destY,
                        NPCoordinateSpace destSpace) {
  LWP_LOG_FUNCTION;
  return 0;
}

NPBool NPN_HandleEvent(NPP id, void* event, NPBool handled) {
  LWP_LOG_FUNCTION;
  return 0;
}

NPBool NPN_UnfocusInstance(NPP id, NPFocusDirection direction) {
  LWP_LOG_FUNCTION;
  return 0;
}

void NPN_URLRedirectResponse(NPP instance, void* notify_data, NPBool allow) {
  LWP_LOG_FUNCTION;
}

void LegacyWebPlugin::InitializeHostFuncs() {
  memset(&host_functions_, 0, sizeof(host_functions_));
  host_functions_.size = sizeof(host_functions_);
  host_functions_.version = (NP_VERSION_MAJOR << 8) | (NP_VERSION_MINOR);

  // The "basic" functions
  host_functions_.geturl = &NPN_GetURL;
  host_functions_.posturl = &NPN_PostURL;
  host_functions_.requestread = &NPN_RequestRead;
  host_functions_.newstream = &NPN_NewStream;
  host_functions_.write = &NPN_Write;
  host_functions_.destroystream = &NPN_DestroyStream;
  host_functions_.status = &NPN_Status;
  host_functions_.uagent = &NPN_UserAgent;
  host_functions_.memalloc = &NPN_MemAlloc;
  host_functions_.memfree = &NPN_MemFree;
  host_functions_.memflush = &NPN_MemFlush;
  host_functions_.reloadplugins = &NPN_ReloadPlugins;

  // Stubs for deprecated Java functions
  host_functions_.getJavaEnv = &NPN_GetJavaEnv;
  host_functions_.getJavaPeer = &NPN_GetJavaPeer;

  // Advanced functions we implement
  host_functions_.geturlnotify = &NPN_GetURLNotify;
  host_functions_.posturlnotify = &NPN_PostURLNotify;
  host_functions_.getvalue = &NPN_GetValue;
  host_functions_.setvalue = &NPN_SetValue;
  host_functions_.invalidaterect = &NPN_InvalidateRect;
  host_functions_.invalidateregion = &NPN_InvalidateRegion;
  host_functions_.forceredraw = &NPN_ForceRedraw;

  // These come from the Javascript Engine
  host_functions_.getstringidentifier = WebBindings::GetStringIdentifier;
  host_functions_.getstringidentifiers = WebBindings::GetStringIdentifiers;
  host_functions_.getintidentifier = WebBindings::GetIntIdentifier;
  host_functions_.identifierisstring = WebBindings::IdentifierIsString;
  host_functions_.utf8fromidentifier = WebBindings::Utf8FromIdentifier;
  host_functions_.intfromidentifier = WebBindings::IntFromIdentifier;
  host_functions_.createobject = WebBindings::CreateObject;
  host_functions_.retainobject = WebBindings::RetainObject;
  host_functions_.releaseobject = WebBindings::ReleaseObject;
  host_functions_.invoke = WebBindings::Invoke;
  host_functions_.invokeDefault = WebBindings::InvokeDefault;
  host_functions_.evaluate = WebBindings::Evaluate;
  host_functions_.getproperty = WebBindings::GetProperty;
  host_functions_.setproperty = WebBindings::SetProperty;
  host_functions_.removeproperty = WebBindings::RemoveProperty;
  host_functions_.hasproperty = WebBindings::HasProperty;
  host_functions_.hasmethod = WebBindings::HasMethod;
  host_functions_.releasevariantvalue = WebBindings::ReleaseVariantValue;
  host_functions_.setexception = WebBindings::SetException;
  host_functions_.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
  host_functions_.poppopupsenabledstate = NPN_PopPopupsEnabledState;
  host_functions_.enumerate = WebBindings::Enumerate;
  host_functions_.pluginthreadasynccall = NPN_PluginThreadAsyncCall;
  host_functions_.construct = WebBindings::Construct;
  host_functions_.getvalueforurl = NPN_GetValueForURL;
  host_functions_.setvalueforurl = NPN_SetValueForURL;
  host_functions_.getauthenticationinfo = NPN_GetAuthenticationInfo;
  host_functions_.scheduletimer = NPN_ScheduleTimer;
  host_functions_.unscheduletimer = NPN_UnscheduleTimer;
  host_functions_.popupcontextmenu = NPN_PopUpContextMenu;
  host_functions_.convertpoint = NPN_ConvertPoint;
  host_functions_.handleevent = NPN_HandleEvent;
  host_functions_.unfocusinstance = NPN_UnfocusInstance;
  host_functions_.urlredirectresponse = NPN_URLRedirectResponse;
}

LegacyWebPlugin::~LegacyWebPlugin() {
  LWP_LOG_MEMBER_FUNCTION;
}

bool LegacyWebPlugin::Initialize(WebPluginContainer* container) {
  LWP_LOG_MEMBER_FUNCTION;

  if (container_ == container)
    return true;  // added to avoid static analyzer warning

  container_ = container;
  libHandle_ = dlopen(library_name_, RTLD_LAZY);
  if (!libHandle_) {
    LWP_LOG_MEMBER_ERROR("Can't load the library '" << library_name_
                                                    << "': " << dlerror());
    return false;
  }
  NP_InitializeFunc NP_Initialize =
      reinterpret_cast<NP_InitializeFunc>(dlsym(libHandle_, "NP_Initialize"));
  if (!NP_Initialize) {
    LWP_LOG_MEMBER_ERROR(
        "Did not get the NP_Initialize function: " << dlerror());
    return false;
  }
  NPError err = NP_Initialize(&host_functions_, &plugin_functions_);
  if (err != NPERR_NO_ERROR) {
    LWP_LOG_MEMBER_ERROR("NP_Initialize failed: " << err);
    return false;
  }

  if (!plugin_functions_.newp) {
    LWP_LOG_MEMBER_ERROR("newp is null");
    return false;
  }

  err = plugin_functions_.newp((NPMIMEType)mime_type_.Utf8().data(), &instance_,
                               mode_, param_count_, param_names_, param_values_,
                               nullptr);
  if (err != NPERR_NO_ERROR) {
    LWP_LOG_MEMBER_ERROR("NPP_New failed: " << err);
    return false;
  }

#if defined(USE_NEVA_PUNCH_HOLE)
  if (container_ != nullptr) {
    if (!solid_color_layer_.get()) {
      solid_color_layer_ = cc::SolidColorLayer::Create();
      container_->SetCcLayer(solid_color_layer_.get(), false);
    }
    solid_color_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);
    solid_color_layer_->SetForceDrawTransparentColor(true);
  }
#endif

  current_instance_ = this;

  return true;
}

void LegacyWebPlugin::Destroy() {
  LWP_LOG_MEMBER_FUNCTION;

  weak_factory_.InvalidateWeakPtrs();
  CloseStreams();

  // JavaScript garbage collection may cause plugin script object references to
  // be retained long after the plugin is destroyed. Some plugins won't cope
  // with their objects being released after they've been destroyed, and once
  // we've actually unloaded the plugin the object's releaseobject() code may
  // no longer be in memory. The container tracks the plugin's objects and lets
  // us invalidate them, releasing the references to them held by the JavaScript
  // runtime.
  if (container_) {
    container_->SetCcLayer(nullptr, true);
    container_ = nullptr;
  }

  if (plugin_functions_.destroy != 0) {
    NPSavedData* savedData = 0;
    plugin_functions_.destroy(GetNPP(), &savedData);
    memset(&plugin_functions_, 0, sizeof(plugin_functions_));

    // TODO: Support savedData.  Technically, these need to be
    //       saved on a per-URL basis, and then only passed
    //       to new instances of the plugin at the same URL.
    //       Sounds like a huge security risk.  When we do support
    //       these, we should pass them back to the PluginLib
    //       to be stored there.
    DCHECK(savedData == 0);
  }

  frame_ = nullptr;

  if (libHandle_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](void* handle) { dlclose(handle); }, libHandle_));
    libHandle_ = nullptr;
  }

  FreeStringArray(param_names_, param_count_);
  FreeStringArray(param_values_, param_count_);

  timers_.clear();

  for (std::size_t file_index = 0; file_index < files_created_.size();
       file_index++) {
    base::DeleteFile(files_created_[file_index], false);
  }

  // this should be the last
  delete this;
}

void LegacyWebPlugin::UpdateGeometry(const blink::WebRect& frame_rect,
                                     const blink::WebRect& clip_rect,
                                     const blink::WebRect& unobscured_rect,
                                     bool is_visible) {
  plugin_rect_ = frame_rect;

  np_window_.x = frame_rect.x;
  np_window_.y = frame_rect.y;

  np_window_.width = frame_rect.width;
  np_window_.height = frame_rect.height;

  if (plugin_functions_.setwindow) {
    plugin_functions_.setwindow(&instance_, &np_window_);
  }
  if (first_geometry_update_pending_ && plugin_url_.IsValid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&LegacyWebPlugin::OnDownloadPluginSrcUrl,
                              main_thread_this_));
  }
  first_geometry_update_pending_ = false;
}

void LegacyWebPlugin::Paint(cc::PaintCanvas* canvas,
                            const blink::WebRect& web_rect) {
#if !defined(USE_NEVA_PUNCH_HOLE)
  gfx::Rect rect(plugin_rect_.x, plugin_rect_.y, plugin_rect_.width,
                 plugin_rect_.height);
  cc::PaintFlags clear_paint;
  clear_paint.setBlendMode(SkBlendMode::kClear);
  canvas->drawRect(gfx::RectToSkRect(rect), clear_paint);
#endif  // USE_NEVA_PUNCH_HOLE
}

v8::Local<v8::Object> LegacyWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  LWP_LOG_MEMBER_FUNCTION;
  NPObject* object = nullptr;

  if (!plugin_functions_.getvalue)
    return v8::Local<v8::Object>();

  NPError npErr = plugin_functions_.getvalue(
      &instance_, NPPVpluginScriptableNPObject, &object);

  if (npErr != NPERR_NO_ERROR)
    return v8::Local<v8::Object>();

  if (object)
    return CreateV8ObjectForNPObject(isolate, object, nullptr);

  return v8::Local<v8::Object>();
}

}  // namespace  blink
