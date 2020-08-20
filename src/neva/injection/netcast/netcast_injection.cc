// Copyright 2014-2019 LG Electronics, Inc.
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

#include "neva/injection/netcast/netcast_injection.h"

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "injection/netcast/netcast_injection.h"
#include "neva/injection/common/public/renderer/injection_webos.h"
#include "neva/injection/grit/injection_resources.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace injections {

namespace {

class NetCastDataManager : public InjectionDataManager {
 public:
  explicit NetCastDataManager(const std::string& json);

  bool GetInitialisedStatus() const {
    return initialized_;
  }

  void SetInitializedStatus(bool status) {
    initialized_ = status;
  }

  void DoInitialize(const std::string& json);

 private:
  static const std::vector<std::string> cached_data_keys_;
  bool initialized_ = false;
};

const std::vector<std::string> NetCastDataManager::cached_data_keys_ = {
  "identifier",
  "launchParams",
  "devicePixelRatio",
  "folderPath"
};

NetCastDataManager::NetCastDataManager(const std::string& json) {
  DoInitialize(json);
}

void NetCastDataManager::DoInitialize(const std::string& json) {
  if (!initialized_ && json != "") {
    Initialize(json, cached_data_keys_);
    initialized_ = true;
  }
}

struct ServiceAllowance {
  std::string app;
  bool devmode;
};

bool CheckLSCallValidation(const std::string& uri, const std::string& app_id) {
  static std::map<std::string, ServiceAllowance> allowed_services = {
    {
      "palm://com.palm.service.tellurium/subscribeToCommands",
      {
        "*",
        true
      }
    }, {
      "palm://com.palm.service.tellurium/notifyEvent",
      {
        "*",
        true
      }
    }, {
      "palm://com.palm.service.tellurium/replyToCommand",
      {
        "*", // "tellerium"
        true // tellerium
      }
    }, {
      "luna://com.webos.service.tv.subtitle/enableSubtitle",
      {
        "mlb",
        false
      },
    }, {
      "palm://com.palm.systemservice/getPreferences",
      {
        "*",  // "test"
        true  // test
      }
    }, {
      "palm://com.webos.service.secondscreen.gateway/app2app/createAppChannel",
      {
        "*",  // "createAppChannel"
        false // second screen
      }
    }
  };

  auto it = allowed_services.find(uri);
  if (it == allowed_services.end())
      return false;

  bool devmode_only = false;
  if (it->second.app == app_id || it->second.app == "*")
    devmode_only = it->second.devmode;

  if (devmode_only &&
      access("/var/luna/preferences/devmode_enabled", F_OK) != 0)
    return false;

  return true;
}

}  // namespace

class NetCastInjection : public gin::Wrappable<NetCastInjection>,
                         public InjectionWebOS {
 public:
  static gin::WrapperInfo kWrapperInfo;

  explicit NetCastInjection(blink::WebLocalFrame* frame);
  NetCastInjection(const NetCastInjection&) = delete;
  NetCastInjection& operator=(const NetCastInjection&) = delete;

  void Exit();
  void Back();
  void LaunchSystemUI(const std::string& param);
  std::string GetMouseOnOff();
  std::string GetURLForCP();
  void SystemKeyboardVisible(bool param);
  void OpenWebBrowser(const std::string& param);
  bool KeepAlive(bool param);
  bool WebOSServiceCall(const std::string& uri,
                        const std::string& param,
                        const std::string& callback,
                        bool fg_only);
  bool WebOSServiceCancel(const std::string& param);
  bool SetCursorImage(gin::Arguments* args);
  std::string Identifier();
  std::string LaunchParams();
  double DevicePixelRatio();
  bool SetDefaultAspectRatio(const std::string& mode);
  bool RequestPromotionStatus(const std::string& param);
  void IsPromotionStatusCallback(gin::Arguments* args);
  void ReloadInjectionData();
  void UpdateInjectionData(const std::string& key, const std::string& value);

 private:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  std::string GetInjectionData(const std::string& name);
  NetCastDataManager data_manager_;
};


gin::WrapperInfo NetCastInjection::kWrapperInfo = {
    gin::kEmbedderNativeGin
};

NetCastInjection::NetCastInjection(blink::WebLocalFrame* frame)
  : InjectionWebOS(frame)
  , data_manager_(CallFunction("netcast,initialize")) {
}

void NetCastInjection::Exit() {
  SendCommand("netcast,NetCastExit");
}

void NetCastInjection::Back() {
  SendCommand("netcast,NetCastBack");
}

void NetCastInjection::LaunchSystemUI(const std::string& param) {
  std::vector<std::string> arguments = { param };
  SendCommand("netcast,NetCastLaunchSystemUI", arguments);
}

std::string NetCastInjection::GetMouseOnOff() {
  const std::string json = CallFunction("netcast,NetCastGetMouseOnOff");
  std::string state("off");
  std::unique_ptr<base::Value> root(base::JSONReader::ReadDeprecated(json));
  if (root.get()) {
    base::DictionaryValue* dict(nullptr);
    if (root->GetAsDictionary(&dict))
      dict->GetString("mouseOnOff", &state);
  }

  return state;
}

std::string NetCastInjection::GetURLForCP() {
  const std::string json = CallFunction("netcast,NetCastGetURLForCP");
  std::string url;
  std::unique_ptr<base::Value> root(base::JSONReader::ReadDeprecated(json));
  if (root.get()) {
    base::DictionaryValue* dict(nullptr);
    if (root->GetAsDictionary(&dict))
      dict->GetString("serverUrl", &url);
  }
  return url;
}

void NetCastInjection::SystemKeyboardVisible(bool param) {
  std::vector<std::string> arguments = { param ? "true" : "false" };
  SendCommand("netcast,NetCastSystemKeyboardVisible", arguments);
}

void NetCastInjection::OpenWebBrowser(const std::string& param) {
  std::vector<std::string> arguments = { param };
  SendCommand("netcast,NetCastOpenWebBrowser", arguments);
}

bool NetCastInjection::KeepAlive(bool param) {
  // Set websetting first because it is same renderer
  setKeepAliveWebApp(param);

  // This function is not related to netcast pluggable
  std::vector<std::string> arguments = { param ? "true" : "false"};
  SendCommand("keepAlive", arguments);
  return true;
}

bool NetCastInjection::WebOSServiceCall(const std::string& uri,
                                        const std::string& param,
                                        const std::string& callback,
                                        bool fg_only) {
  if (!CheckLSCallValidation(uri, GetInjectionData("identifier")))
    return false;

  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue());
  root->SetString("uri", uri);
  root->SetString("param", param);
  root->SetString("callback", callback);
  root->SetBoolean("fgOnly", fg_only);

  std::string output_json;
  base::JSONWriter::Write(*root.get(), &output_json);

  std::vector<std::string> arguments = { output_json };
  SendCommand("netcast,webOSServiceCall", arguments);
  return true;
}

bool NetCastInjection::WebOSServiceCancel(const std::string& param) {
  if (!CheckLSCallValidation(param, GetInjectionData("identifier")))
    return false;

  std::vector<std::string> arguments = { arguments };
  SendCommand("netcast,webOSCancelServiceCall", arguments);
  return true;
}

bool NetCastInjection::SetCursorImage(gin::Arguments* args) {
  // setCursor("default")
  // setCursor("blank")
  // setCursor("filePath")
  // setCursor("filePath", hotspot_x, hotspot_y)
  if (args->Length() != 1 && args->Length() != 3)
    return false;

  std::string cursor_arg;
  if (!args->GetNext(&cursor_arg))
    return false;

  // hotspot parameters expect v8 int, but we need to convert
  // them to string for passing to browser process through
  // SendCommand
  std::string x_str = "-1";
  std::string y_str = "-1";

  if (args->Length() == 3) {
    int x, y;
    if (!args->GetNext(&x) || !args->GetNext(&y))
      return false;

    x_str = std::to_string(x);
    y_str = std::to_string(y);
  }

  // for a security issue, only app installed file path is allowed
  // set custom cursor
  //    NetCast.netCastSetCursor("/usr/palm/application/myapp/cursor.png");
  //    NetCast.netCastSetCursor("file:///usr/palm/application/myapp/cursor.png");
  //    NetCast.netCastSetCursor("image/cursor.png");
  // set custom cursor with hot spot (pointing tip)
  //    NetCast.netCastSetCursor("image/cursor.png", 9, 10);
  // restore to default system setting
  //    NetCast.netCastSetCursor("default");
  //    NetCast.netCastSetCursor("");
  if (cursor_arg.empty())
    cursor_arg = "default";
  else if (cursor_arg != "default" && cursor_arg != "blank") {
    // custom cursor : need to check file path
    cursor_arg = checkFileValidation(cursor_arg, GetInjectionData("folderPath"));
    if (cursor_arg.empty())
      return false;
  }

  std::vector<std::string> arguments = { cursor_arg, x_str, y_str };
  SendCommand("netcast,NetCastSetCursorImage", arguments);
  return true;
}

std::string NetCastInjection::Identifier() {
  return GetInjectionData("identifier");
}

std::string NetCastInjection::LaunchParams() {
  return GetInjectionData("launchParams");
}

double NetCastInjection::DevicePixelRatio() {
  std::string result = GetInjectionData("devicePixelRatio");
  return result.empty() ? 1. : std::stod(result);
}

bool NetCastInjection::SetDefaultAspectRatio(const std::string& mode) {
  if(mode == "original" || mode == "zoom" || mode == "full") {
    std::vector<std::string> arguments = { mode };
    SendCommand("netcast,NetCastSetDefaultAspectRatio", arguments);
    return true;
  }
  return false;
}

bool NetCastInjection::RequestPromotionStatus(const std::string& param) {
  std::vector<std::string> arguments = { param };
  SendCommand("netcast,NetCastIsPromotionStatus", arguments);
  return true;
}

void NetCastInjection::IsPromotionStatusCallback(
    gin::Arguments* args) {
  v8::Local<v8::Array> result =
      v8::Array::New(args->isolate(), args->Length());
  v8::Local<v8::Value> item;
  int32_t i = 0;
  v8::Local<v8::Context> context = args->GetHolderCreationContext();
  while (args->GetNext(&item)) {
    result->Set(context,
                gin::Converter<int32_t>::ToV8(args->isolate(), i++), item);
  }
  v8::Local<v8::Value> ret(result);
  args->Return(ret);
}

void NetCastInjection::ReloadInjectionData() {
  data_manager_.SetInitializedStatus(false);
  const std::string json = CallFunction("netcast,initialize");
  data_manager_.DoInitialize(json);
}

void NetCastInjection::UpdateInjectionData(
    const std::string& key, const std::string& value) {
  data_manager_.UpdateInjectionData(key, value);
}

gin::ObjectTemplateBuilder NetCastInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NetCastInjection>::GetObjectTemplateBuilder(isolate)
      .SetMethod("netCastExit", &NetCastInjection::Exit)
      .SetMethod("netCastBack", &NetCastInjection::Back)
      .SetMethod("LaunchSystemUI", &NetCastInjection::LaunchSystemUI)
      .SetMethod("netCastGetMouseOnOff", &NetCastInjection::GetMouseOnOff)
      .SetMethod("netCastGetURLForCP", &NetCastInjection::GetURLForCP)
      .SetMethod("netCastSystemKeyboardVisible",
                 &NetCastInjection::SystemKeyboardVisible)
      .SetMethod("netCastOpenWebBrowser", &NetCastInjection::OpenWebBrowser)
      .SetMethod("netCastKeepAlive", &NetCastInjection::KeepAlive)
      .SetMethod("webOSServiceCall", &NetCastInjection::WebOSServiceCall)
      .SetMethod("webOSServiceCancel", &NetCastInjection::WebOSServiceCancel)
      .SetMethod("netCastSetCursorImage", &NetCastInjection::SetCursorImage)
      .SetMethod("netCastIdentifier", &NetCastInjection::Identifier)
      .SetMethod("netCastLaunchParams", &NetCastInjection::LaunchParams)
      .SetMethod("devicePixelRatio", &NetCastInjection::DevicePixelRatio)
      .SetMethod("netCastSetDefaultAspectRatio",
                 &NetCastInjection::SetDefaultAspectRatio)
      .SetMethod("netCastIsPromotionStatus",
                 &NetCastInjection::RequestPromotionStatus)
      .SetMethod("_isPromotionStatusCallback",
                 &NetCastInjection::IsPromotionStatusCallback)
      .SetMethod("reloadInjectionData", &NetCastInjection::ReloadInjectionData)
      .SetMethod("updateInjectionData", &NetCastInjection::UpdateInjectionData);
 }

std::string NetCastInjection::GetInjectionData(const std::string& name) {
  if (!data_manager_.GetInitialisedStatus()) {
    const std::string json = CallFunction("netcast,initialize");
    data_manager_.DoInitialize(json);
  }

  if (data_manager_.GetInitialisedStatus()) {
    std::string result;
    if (data_manager_.GetInjectionData(name, result))
      return result;
  }

  return CallFunction(name);
}

const char NetCastWebAPI::kInjectionName[] = "v8/netcast";

// static
void NetCastWebAPI::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> netcast_value =
      global->Get(context, gin::StringToV8(isolate, "NetCast"))
          .ToLocalChecked();
  if (!netcast_value.IsEmpty() && netcast_value->IsObject())
    return;

  v8::Local<v8::Object> netcast;
  if (!CreateNetCastObject(frame, isolate, global).ToLocal(&netcast))
    return;

  NetCastInjection* netcast_injection = nullptr;
  if (gin::Converter<NetCastInjection*>::FromV8(
          isolate, netcast, &netcast_injection)) {
    const std::string extra_objects_js =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_NETCAST_INJECTION_JS);

    v8::Local<v8::Script> script;
    if (v8::Script::Compile(
            context, gin::StringToV8(
                isolate, extra_objects_js.c_str())).ToLocal(&script))
      script->Run(context);
  }
}

void NetCastWebAPI::Uninstall(blink::WebLocalFrame* frame) {
  const std::string extra_objects_js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_NETCAST_ROLLBACK_JS);

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Script> local_script;
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
      context,
      gin::StringToV8(isolate, extra_objects_js.c_str()));

  if (script.ToLocal(&local_script))
    local_script->Run(context);
}

// static
v8::MaybeLocal<v8::Object> NetCastWebAPI::CreateNetCastObject(
    blink::WebLocalFrame* frame,
    v8::Isolate* isolate,
    v8::Local<v8::Object> parent) {
  gin::Handle<NetCastInjection> netcast =
      gin::CreateHandle(isolate, new NetCastInjection(frame));
  parent
      ->Set(frame->MainWorldScriptContext(),
            gin::StringToV8(isolate, "NetCast"), netcast.ToV8())
      .Check();
  return netcast->GetWrapper(isolate);
}

}  // namespace injections
