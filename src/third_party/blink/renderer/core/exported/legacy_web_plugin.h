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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_H_

#include "neva/npapi/bindings/npapi.h"
#include "neva/npapi/bindings/nphostapi.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/renderer/core/exported/np_plugin.h"

#if defined(USE_NEVA_PUNCH_HOLE)
#include "cc/layers/solid_color_layer.h"
#endif  // USE_NEVA_PUNCH_HOLE

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class PluginStreamUrl;
class WebDragData;
class WebFrame;
class WebInputEvent;
class WebPluginContainer;
class WebURL;
class WebURLRequest;
class WebURLResponse;

class LegacyWebPlugin : public WebPlugin, public NPPlugin {
 public:
  static LegacyWebPlugin* TryCreateLegacyWebPlugin(
      const String& mime_type,
      blink::WebFrame*,
      const blink::WebPluginParams&);

  // WebPlugin methods:
  bool Initialize(blink::WebPluginContainer*) override;
  WebPluginContainer* Container() const override { return container_; }
  void Destroy() override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate*) override;
  bool CanProcessDrag() const override { return false; }
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason) override {}
  void Paint(cc::PaintCanvas*, const WebRect&) override;
  void UpdateGeometry(const blink::WebRect& frame_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      bool isVisible) override;
  void UpdateFocus(bool, mojom::FocusType) override {}
  void UpdateVisibility(bool) override {}
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&,
                                       ui::Cursor*) override {
    return WebInputEventResult::kNotHandled;
  }
  bool HandleDragStatusUpdate(WebDragStatus,
                              const WebDragData&,
                              WebDragOperationsMask,
                              const gfx::PointF& position,
                              const gfx::PointF& screen_position) override {
    return false;
  }
  void DidReceiveResponse(const blink::WebURLResponse&) override {}
  void DidReceiveData(const char* data, size_t data_length) override {}
  void DidFinishLoading() override {}
  void DidFailLoading(const blink::WebURLError&) override {}
#if defined(USE_NEVA_MEDIA)
  void DidSuppressMediaPlay(bool suppressed) override;
#endif
  bool IsPlaceholder() override { return false; }

  static LegacyWebPlugin* CurrentLegacyWebPlugin();

  // NPN_* functions
  NPError GetValue(NPNVariable variable, void* value);
  NPError SetValue(NPPVariable variable, void* value);
  NPError GetURLNotify(const char* url,
                       const char* target,
                       bool,
                       void* notify_data);
  // Helper that implements NPN_PluginThreadAsyncCall semantics
  void PluginThreadAsyncCall(void (*func)(void*), void* user_data);
  uint32_t ScheduleTimer(uint32_t interval,
                         NPBool repeat,
                         void (*func)(NPP id, uint32_t timer_id));

  void UnscheduleTimer(uint32_t timer_id);
  const char* UserAgent();
  void OnTimerCall(void (*func)(NPP id, uint32_t timer_id),
                   NPP id,
                   uint32_t timer_id);

  // NPPlugin methods:
  NPP GetNPP() override;
  NPError NPP_NewStream(NPMIMEType,
                        NPStream*,
                        NPBool,
                        unsigned short*) override;
  NPError NPP_DestroyStream(NPStream*, NPReason) override;
  int NPP_WriteReady(NPStream*) override;
  int NPP_Write(NPStream*, int, int, void*) override;
  void NPP_StreamAsFile(NPStream*, const char*) override;
  void NPP_URLNotify(const char*, NPReason, void*) override;
  void NPP_URLRedirectNotify(const char* url,
                             int32_t status,
                             void* notify_data) override;
  void RemoveStream(PluginStreamUrl*) override;
  void CancelResource(unsigned long) override;
  void SetDeferLoading(unsigned long, bool) override;
  bool handles_url_redirects() const override;

  bool IsValidStream(const NPStream* stream);
  void CloseStreams();

 protected:
  ~LegacyWebPlugin() override;

 private:
  explicit LegacyWebPlugin(blink::WebFrame*,
                           const blink::WebPluginParams&,
                           const char* library_name);

  class LoaderClient;
  void InitializeHostFuncs();
  void SetParameters(const WebVector<WebString>& paramNames,
                     const WebVector<WebString>& paramValues);
  void FreeStringArray(char** stringArray, int length);
  void OnPluginThreadAsyncCall(void (*func)(void*), void* user_data);
  void OnDownloadPluginSrcUrl();

  void RemoveClient(size_t i);
  unsigned long GetNextResourceId();

  static LegacyWebPlugin* current_instance_;

  WebFrame* frame_;
  WebPluginContainer* container_ = nullptr;
  void* libHandle_ = nullptr;
  blink::WebRect plugin_rect_;
  NPNetscapeFuncs host_functions_;
  NPPluginFuncs plugin_functions_;
  std::vector<scoped_refptr<PluginStreamUrl>> open_streams_;
  bool in_close_streams_ = false;
  std::vector<std::unique_ptr<LoaderClient>> clients_;
  NPP_t instance_;
  int mode_;
  WebURL plugin_url_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  WebString mime_type_;
  int param_count_ = 0;
  char** param_names_ = nullptr;
  char** param_values_ = nullptr;
  char library_name_[255];
  bool is_windowed_ = false;
  bool is_transparent_ = false;
  bool first_geometry_update_pending_ = true;
  NPWindow np_window_;
#if defined(USE_NEVA_PUNCH_HOLE)
  scoped_refptr<cc::SolidColorLayer> solid_color_layer_;
#endif  // USE_NEVA_PUNCH_HOLE

  // List of files created for the current plugin instance. File names are
  // added to the list every time the NPP_StreamAsFile function is called.
  std::vector<base::FilePath> files_created_;

  // Next unusued timer id.
  uint32_t next_timer_id_ = 0;

  // Map of timer id to settings for timer.
  struct TimerInfo {
    uint32_t interval;
    bool repeat;
  };
  typedef std::map<uint32_t, TimerInfo> TimerMap;
  TimerMap timers_;
  base::WeakPtr<LegacyWebPlugin> main_thread_this_;
  base::WeakPtrFactory<LegacyWebPlugin> weak_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_H_
