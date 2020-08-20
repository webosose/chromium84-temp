// Copyright 2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/injection/webosgavplugin/webosgavplugin_injection.h"

#include <map>
#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/public/common/neva/frame_video_window_factory.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "neva/injection/common/public/renderer/injection_webos.h"
#include "neva/injection/grit/injection_resources.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/platform_window/neva/mojom/video_window_controller.mojom.h"

#define PLUGIN_NAME "webosgavplugin"

namespace injections {

namespace {

inline bool IsTrue(v8::Maybe<bool> maybe) {
  return maybe.IsJust() && maybe.FromJust();

}  // namespace

class WebOSGAVDataManager : public InjectionDataManager {
 public:
  explicit WebOSGAVDataManager(const std::string& json);

  bool GetInitialisedStatus() const { return initialized_; }

  void SetInitializedStatus(bool status) { initialized_ = status; }

  void DoInitialize(const std::string& json);

 private:
  bool initialized_ = false;
};

WebOSGAVDataManager::WebOSGAVDataManager(const std::string& json) {
  DoInitialize(json);
}

void WebOSGAVDataManager::DoInitialize(const std::string& json) {
  if (!initialized_ && json != "") {
    Initialize(json, std::vector<std::string>());
    initialized_ = true;
  }
}

}  // namespace

class VideoWindowClientOwner {
 public:
  virtual void OnVideoWindowCreated(const std::string& id,
                                    const std::string& video_window_id,
                                    int type) = 0;
  virtual void OnVideoWindowDestroyed(const std::string& id) = 0;
};

struct VideoWindowInfo {
  VideoWindowInfo(const base::UnguessableToken& id, const std::string& nid)
      : window_id(id), native_window_id(nid) {}
  base::UnguessableToken window_id;
  std::string native_window_id;
};

class VideoWindowImpl : public ui::mojom::VideoWindowClient {
 public:
  VideoWindowImpl(
      VideoWindowClientOwner* owner,
      const std::string& id,
      int type,
      mojo::PendingRemote<ui::mojom::VideoWindow> window_remote,
      mojo::PendingReceiver<ui::mojom::VideoWindowClient> pending_receiver)
      : owner_(owner),
        id_(id),
        type_(type),
        window_remote_(std::move(window_remote)) {
    receiver_.Bind(std::move(pending_receiver));
    VLOG(1) << "[" << this << "] " << __func__;
  }

  ~VideoWindowImpl() { VLOG(1) << "[" << this << "] " << __func__; }

  void OnVideoWindowCreated(const ui::VideoWindowInfo& info) final {
    VLOG(1) << __func__ << " id=" << id_;
    info_ = info;
    owner_->OnVideoWindowCreated(id_, info.native_window_id, type_);
  }

  void OnVideoWindowDestroyed() final {
    VLOG(1) << __func__ << " id=" << id_;
    owner_->OnVideoWindowDestroyed(id_);
  }

  // Not used in gav
  void OnVideoWindowGeometryChanged(const gfx::Rect& rect) final {}
  // Not used in gav
  void OnVideoWindowVisibilityChanged(bool visibility) final {}

  ui::mojom::VideoWindow* GetVideoWindow() { return window_remote_.get(); }

  VideoWindowClientOwner* owner_;
  std::string id_;
  int type_;
  mojo::Remote<ui::mojom::VideoWindow> window_remote_;
  mojo::Receiver<ui::mojom::VideoWindowClient> receiver_{this};
  ui::VideoWindowInfo info_{base::UnguessableToken::Null(), std::string()};
};

class WebOSGAVInjection : public VideoWindowClientOwner,
                          public gin::Wrappable<WebOSGAVInjection>,
                          public InjectionWebOS {
 public:
  static gin::WrapperInfo kWrapperInfo;

  explicit WebOSGAVInjection(blink::WebLocalFrame* frame);
  WebOSGAVInjection(const WebOSGAVInjection&) = delete;
  WebOSGAVInjection& operator=(const WebOSGAVInjection&) = delete;

  std::string gavGetMediaId();
  bool gavRequestMediaLayer(const std::string& mid, int type);

  void gavUpdateMediaLayerBounds(const std::string& mid,
                                 int src_x,
                                 int src_y,
                                 int src_w,
                                 int src_h,
                                 int dst_x,
                                 int dst_y,
                                 int dst_w,
                                 int dst_h);

  void gavDestroyMediaLayer(const std::string& mid);
  void gavUpdateMediaCropBounds(const std::string& mid,
                                int ori_x,
                                int ori_y,
                                int ori_w,
                                int ori_h,
                                int src_x,
                                int src_y,
                                int src_w,
                                int src_h,
                                int dst_x,
                                int dst_y,
                                int dst_w,
                                int dst_h);
  void gavSetMediaProperty(const std::string& mid,
                           const std::string& name,
                           const std::string& value);

  void ReloadInjectionData();
  void UpdateInjectionData(const std::string& key, const std::string& value);

  // Implements VideoWindowClientOwner
  void OnVideoWindowCreated(const std::string& id,
                            const std::string& video_window_id,
                            int type) final;
  void OnVideoWindowDestroyed(const std::string& id) final;

 private:
  static const char kPluginName[];
  static const char kOnCreatedMediaLayerName[];
  static const char kWillDestroyMediaLayer[];

  void gavMediaLayerDestroyed(const std::string& id);

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  std::string GetInjectionData(const std::string& name);
  WebOSGAVDataManager data_manager_;
  content::mojom::FrameVideoWindowFactory* video_window_factory_;
  unsigned int next_plugin_media_id_ = 0;
  std::map<std::string, VideoWindowImpl> id_to_window_;
};

gin::WrapperInfo WebOSGAVInjection::kWrapperInfo = {gin::kEmbedderNativeGin};

const char WebOSGAVInjection::kPluginName[] = PLUGIN_NAME;
const char WebOSGAVInjection::kOnCreatedMediaLayerName[] =
    "onCreatedMediaLayer";
const char WebOSGAVInjection::kWillDestroyMediaLayer[] =
    "willDestroyMediaLayer";

WebOSGAVInjection::WebOSGAVInjection(blink::WebLocalFrame* frame)
    : InjectionWebOS(frame),
      data_manager_(CallFunction("initialize")),
      video_window_factory_(content::RenderFrame::FromWebFrame(frame)
                                ->GetFrameVideoWindowFactory()) {}

void WebOSGAVInjection::OnVideoWindowCreated(const std::string& id,
                                             const std::string& video_window_id,
                                             int type) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper))
    return;

  v8::Local<v8::Context> context = wrapper->CreationContext();
  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> object_key = gin::StringToV8(isolate, kPluginName);

  v8::Local<v8::Object> plugin;
  if (!gin::Converter<v8::Local<v8::Object>>::FromV8(
          isolate, global->Get(context, object_key).ToLocalChecked(),
          &plugin)) {
    LOG(ERROR) << __func__ << " fail to get plugin object";
    return;
  }

  v8::Local<v8::String> callback_key =
      gin::StringToV8(isolate, kOnCreatedMediaLayerName);
  if (!IsTrue(plugin->Has(context, callback_key))) {
    LOG(ERROR) << __func__ << " No " << kOnCreatedMediaLayerName;
    return;
  }

  v8::Local<v8::Function> callback;
  if (!gin::Converter<v8::Local<v8::Function>>::FromV8(
          isolate, plugin->Get(context, callback_key).ToLocalChecked(),
          &callback)) {
    LOG(ERROR) << __func__ << " Convert to function error";
    return;
  }

  const int argc = 3;
  v8::Local<v8::Value> argv[] = {gin::StringToV8(isolate, id),
                                 gin::StringToV8(isolate, video_window_id),
                                 gin::ConvertToV8(isolate, type)};
  ALLOW_UNUSED_LOCAL(callback->Call(context, wrapper, argc, argv));
}

void WebOSGAVInjection::OnVideoWindowDestroyed(const std::string& id) {}

std::string WebOSGAVInjection::gavGetMediaId() {
  std::stringstream ss;
  ss << "plugin_media_" << next_plugin_media_id_++;
  std::string id = ss.str();
  VLOG(1) << __func__ << " id=" << id;
  return ss.str();
}

bool WebOSGAVInjection::gavRequestMediaLayer(const std::string& id, int type) {
  VLOG(1) << __func__ << " id=" << id << " type=" << type;
  if (id_to_window_.find(id) != id_to_window_.end()) {
    LOG(ERROR) << __func__ << " already requested";
    return false;
  }

  mojo::PendingRemote<ui::mojom::VideoWindow> window_remote;
  mojo::PendingRemote<ui::mojom::VideoWindowClient> client_remote;
  mojo::PendingReceiver<ui::mojom::VideoWindowClient> client_receiver(
      client_remote.InitWithNewPipeAndPassReceiver());

  // TODO(neva): setup disconnect_handler
  video_window_factory_->CreateVideoWindow(
      std::move(client_remote), window_remote.InitWithNewPipeAndPassReceiver(),
      {false, true});

  auto result = id_to_window_.emplace(
      std::piecewise_construct, std::forward_as_tuple(id),
      std::forward_as_tuple(this, id, type, std::move(window_remote),
                            std::move(client_receiver)));
  result.first->second.window_remote_.set_disconnect_handler(base::BindOnce(
      &WebOSGAVInjection::gavMediaLayerDestroyed, base::Unretained(this), id));

  return true;
}

// Destroyed from video window
void WebOSGAVInjection::gavMediaLayerDestroyed(const std::string& id) {
  auto it = id_to_window_.find(id);
  if (it == id_to_window_.end()) {
    return;
  }
  id_to_window_.erase(it);

  // Notify to plugin
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper))
    return;

  v8::Local<v8::Context> context = wrapper->CreationContext();
  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> object_key = gin::StringToV8(isolate, kPluginName);

  v8::Local<v8::Object> plugin;
  if (!gin::Converter<v8::Local<v8::Object>>::FromV8(
          isolate, global->Get(context, object_key).ToLocalChecked(),
          &plugin)) {
    LOG(ERROR) << __func__ << " fail to get plugin object";
    return;
  }

  v8::Local<v8::String> callback_key =
      gin::StringToV8(isolate, kWillDestroyMediaLayer);
  if (!IsTrue(plugin->Has(context, callback_key))) {
    LOG(ERROR) << __func__ << " No " << kWillDestroyMediaLayer;
    return;
  }

  v8::Local<v8::Function> callback;
  if (!gin::Converter<v8::Local<v8::Function>>::FromV8(
          isolate, plugin->Get(context, callback_key).ToLocalChecked(),
          &callback)) {
    LOG(ERROR) << __func__ << " Convert to function error";
    return;
  }

  const int argc = 1;
  v8::Local<v8::Value> argv[] = {gin::StringToV8(isolate, id)};
  ALLOW_UNUSED_LOCAL(callback->Call(context, wrapper, argc, argv));
}

void WebOSGAVInjection::gavUpdateMediaLayerBounds(const std::string& id,
                                                  int src_x,
                                                  int src_y,
                                                  int src_w,
                                                  int src_h,
                                                  int dst_x,
                                                  int dst_y,
                                                  int dst_w,
                                                  int dst_h) {
  VLOG(1) << __func__ << " id=" << id << " src(" << src_x << "," << src_y
          << ") " << src_w << "x" << src_h << " dst(" << dst_x << "," << dst_y
          << ") " << dst_w << "x" << dst_h;
  auto it = id_to_window_.find(id);
  if (it == id_to_window_.end()) {
    LOG(ERROR) << __func__ << " failed to find id=" << id;
    return;
  }
  it->second.GetVideoWindow()->UpdateVideoWindowGeometry(
      gfx::Rect(src_x, src_y, src_w, src_h),
      gfx::Rect(dst_x, dst_y, dst_w, dst_h));
}

void WebOSGAVInjection::gavUpdateMediaCropBounds(const std::string& id,
                                                 int ori_x,
                                                 int ori_y,
                                                 int ori_w,
                                                 int ori_h,
                                                 int src_x,
                                                 int src_y,
                                                 int src_w,
                                                 int src_h,
                                                 int dst_x,
                                                 int dst_y,
                                                 int dst_w,
                                                 int dst_h) {
  VLOG(1) << __func__ << " id=" << id << " ori(" << ori_x << "," << ori_y
          << ") " << ori_w << "x" << ori_h << " src(" << src_x << "," << src_y
          << ") " << src_w << "x" << src_h << " dst(" << dst_x << "," << dst_y
          << ") " << dst_w << "x" << dst_h;
  auto it = id_to_window_.find(id);
  if (it == id_to_window_.end()) {
    LOG(ERROR) << __func__ << " failed to find id=" << id;
    return;
  }
  it->second.GetVideoWindow()->UpdateVideoWindowGeometryWithCrop(
      gfx::Rect(ori_x, ori_y, ori_w, ori_h),
      gfx::Rect(src_x, src_y, src_w, src_h),
      gfx::Rect(dst_x, dst_y, dst_w, dst_h));
}

// Destroy video window from plugin
void WebOSGAVInjection::gavDestroyMediaLayer(const std::string& id) {
  VLOG(1) << __func__ << " id=" << id;
  // Erase will destruct VideoWindowImpl then mojo pipe will be closed.
  id_to_window_.erase(id);
}

void WebOSGAVInjection::gavSetMediaProperty(const std::string& id,
                                            const std::string& name,
                                            const std::string& value) {
  VLOG(1) << __func__ << " id=" << id << " name=" << name << " value=" << value;
  auto it = id_to_window_.find(id);
  if (it == id_to_window_.end()) {
    LOG(ERROR) << __func__ << " failed to find id=" << id;
    return;
  }
  it->second.GetVideoWindow()->SetProperty(name, value);
}

void WebOSGAVInjection::ReloadInjectionData() {
  data_manager_.SetInitializedStatus(false);
  const std::string json = CallFunction("initialize");
  data_manager_.DoInitialize(json);
}

void WebOSGAVInjection::UpdateInjectionData(const std::string& key,
                                            const std::string& value) {
  data_manager_.UpdateInjectionData(key, value);
}

gin::ObjectTemplateBuilder WebOSGAVInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WebOSGAVInjection>::GetObjectTemplateBuilder(isolate)
      .SetMethod("gavGetMediaId", &WebOSGAVInjection::gavGetMediaId)
      .SetMethod("gavRequestMediaLayer",
                 &WebOSGAVInjection::gavRequestMediaLayer)
      .SetMethod("gavUpdateMediaLayerBounds",
                 &WebOSGAVInjection::gavUpdateMediaLayerBounds)
      .SetMethod("gavDestroyMediaLayer",
                 &WebOSGAVInjection::gavDestroyMediaLayer)
      .SetMethod("gavUpdateMediaCropBounds",
                 &WebOSGAVInjection::gavUpdateMediaCropBounds)
      .SetMethod("gavSetMediaProperty", &WebOSGAVInjection::gavSetMediaProperty)
      .SetMethod("reloadInjectionData", &WebOSGAVInjection::ReloadInjectionData)
      .SetMethod("updateInjectionData",
                 &WebOSGAVInjection::UpdateInjectionData);
}

std::string WebOSGAVInjection::GetInjectionData(const std::string& name) {
  if (!data_manager_.GetInitialisedStatus()) {
    const std::string json = CallFunction("webosgavplugin,initialize");
    data_manager_.DoInitialize(json);
  }

  if (data_manager_.GetInitialisedStatus()) {
    std::string result;
    if (data_manager_.GetInjectionData(name, result))
      return result;
  }

  return CallFunction(name);
}

const char WebOSGAVWebAPI::kInjectionName[] = "v8/" PLUGIN_NAME;
const char WebOSGAVWebAPI::kInjectionObjectName[] = "WebOSGAVInternal_";

void WebOSGAVWebAPI::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> webosgavplugin_value =
      global->Get(context, gin::StringToV8(isolate, kInjectionObjectName))
          .ToLocalChecked();
  if (!webosgavplugin_value.IsEmpty() && webosgavplugin_value->IsObject())
    return;

  v8::Local<v8::Object> webosgavplugin;
  if (!CreateWebOSGAVObject(frame, isolate, global).ToLocal(&webosgavplugin))
    return;

  WebOSGAVInjection* webosgavplugin_injection = nullptr;
  if (gin::Converter<WebOSGAVInjection*>::FromV8(isolate, webosgavplugin,
                                                 &webosgavplugin_injection)) {
    const std::string extra_objects_js =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_WEBOSGAVPLUGIN_INJECTION_JS);

    v8::Local<v8::Script> script;
    if (v8::Script::Compile(context,
                            gin::StringToV8(isolate, extra_objects_js.c_str()))
            .ToLocal(&script))
      script->Run(context);
  }
}

// static
void WebOSGAVWebAPI::Uninstall(blink::WebLocalFrame* frame) {
  const std::string extra_objects_js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEBOSGAVPLUGIN_ROLLBACK_JS);

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Script> local_script;
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
      context, gin::StringToV8(isolate, extra_objects_js.c_str()));

  if (script.ToLocal(&local_script))
    local_script->Run(context);
}

// static
v8::MaybeLocal<v8::Object> WebOSGAVWebAPI::CreateWebOSGAVObject(
    blink::WebLocalFrame* frame,
    v8::Isolate* isolate,
    v8::Local<v8::Object> parent) {
  gin::Handle<WebOSGAVInjection> webosgavplugin =
      gin::CreateHandle(isolate, new WebOSGAVInjection(frame));
  parent
      ->Set(frame->MainWorldScriptContext(),
            gin::StringToV8(isolate, kInjectionObjectName),
            webosgavplugin.ToV8())
      .Check();
  return webosgavplugin->GetWrapper(isolate);
}

}  // namespace injections
