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

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/npapi/np_v8_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_impl.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_priv.h"
#include "third_party/blink/renderer/bindings/core/npapi/v8_np_object.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"

namespace blink {
namespace neva {

static NPObject* CreateNoScriptObject() {
  NOTIMPLEMENTED();
  return nullptr;
}

template <typename original_t>
const original_t* ScriptControllerMixIn<original_t>::GetSelf() {
  return static_cast<const original_t*>(this);
}

template <typename original_t>
void ScriptControllerMixIn<original_t>::Trace(Visitor* visitor) {
  visitor->Trace(plugin_objects_);
}

template <typename original_t>
bool ScriptControllerMixIn<original_t>::BindToWindowObject(LocalFrame* frame,
    const WTF::String& key, NPObject* object) {
  ScriptState* scriptState = ToScriptStateForMainWorld(frame);
  if (!scriptState)
    return false;

  ScriptState::Scope scope(scriptState);
  v8::Local<v8::Object> value = blink::CreateV8ObjectForNPObject(
      GetSelf()->GetIsolate(), object, 0);

  // Attach to the global object.
  bool result;
  return GetSelf()
             ->GetIsolate()
             ->GetCurrentContext()
             ->Global()
             ->Set(GetSelf()->GetIsolate()->GetCurrentContext(),
                   blink::V8String(GetSelf()->GetIsolate(), key), value)
             .To(&result) &&
         result;
}

template <typename original_t>
void ScriptControllerMixIn<original_t>::ClearScriptObjects() {
  PluginObjectMap::iterator it = plugin_objects_.begin();
  for (; it != plugin_objects_.end(); ++it) {
    _NPN_UnregisterObject(it->value);
    _NPN_ReleaseObject(it->value);
  }
  plugin_objects_.clear();
  VLOG(2) << __PRETTY_FUNCTION__ << ": window_script_npobject="
      << static_cast<void*>(window_script_npobject_);

  if (window_script_npobject_) {
    blink::DisposeUnderlyingV8Object(GetSelf()->GetIsolate(), window_script_npobject_);
    _NPN_ReleaseObject(window_script_npobject_);
    window_script_npobject_ = nullptr;
  }
}

template <typename original_t>
void ScriptControllerMixIn<original_t>::CleanupScriptObjectsForPlugin(
    blink::WebPluginContainerImpl* nativeHandle) {
  PluginObjectMap::iterator it = plugin_objects_.find(nativeHandle);
  if (it == plugin_objects_.end())
    return;
  _NPN_UnregisterObject(it->value);
  _NPN_ReleaseObject(it->value);
  plugin_objects_.erase(it);
}

template <typename original_t>
NPObject* ScriptControllerMixIn<original_t>::CreateScriptObjectForPluginElement(
    blink::HTMLPlugInElement* plugin) {
  // Can't create NPObjects when JavaScript is disabled.
  if (!GetSelf()->GetFrame()->GetDocument()
      ->CanExecuteScripts(blink::kNotAboutToExecuteScript))
    return CreateNoScriptObject();

  ScriptState* scriptState = ToScriptStateForMainWorld(GetSelf()->GetFrame());
  if (!scriptState)
    return CreateNoScriptObject();

  ScriptState::Scope scope(scriptState);
  blink::LocalDOMWindow* window =
      blink::ToLocalDOMWindow(GetSelf()->GetIsolate()->GetCurrentContext());
  v8::Local<v8::Value> v8plugin =
      ToV8(plugin, GetSelf()->GetIsolate()->GetCurrentContext()->Global(),
           scriptState->GetIsolate());
  if (v8plugin.IsEmpty() || !v8plugin->IsObject())
    return CreateNoScriptObject();
  NPObject* np_object =
      NPCreateV8ScriptObject(scriptState->GetIsolate(), 0,
                             v8::Local<v8::Object>::Cast(v8plugin), window);
  _NPN_RegisterObject(np_object, nullptr);
  return np_object;
}

static NPObject* CreateScriptObject(blink::LocalFrame* frame,
                                    v8::Isolate* isolate) {
  ScriptState* scriptState = ToScriptStateForMainWorld(frame);
  if (!scriptState)
    return CreateNoScriptObject();

  ScriptState::Scope scope(scriptState);
  blink::LocalDOMWindow* window = frame->DomWindow();
  v8::Local<v8::Value> global = ToV8(window,
                                      scriptState->GetContext()->Global(),
                                      scriptState->GetIsolate());
  if (global.IsEmpty())
    return CreateNoScriptObject();
  assert(global->IsObject());
  return NPCreateV8ScriptObject(isolate, 0, v8::Local<v8::Object>::Cast(global),
                                window);
}

template <typename original_t>
NPObject* ScriptControllerMixIn<original_t>::GetWindowScriptNPObject() {
  if (window_script_npobject_)
    return window_script_npobject_;

  if (GetSelf()->GetFrame()->GetDocument()
      ->CanExecuteScripts(blink::kNotAboutToExecuteScript)) {
    // JavaScript is enabled, so there is a JavaScript window object.
    // Return an NPObject bound to the window object.
    window_script_npobject_ = CreateScriptObject(GetSelf()->GetFrame(),
        GetSelf()->GetIsolate());
    _NPN_RegisterObject(window_script_npobject_, nullptr);
  } else {
    // JavaScript is not enabled, so we cannot bind the NPObject to the
    // JavaScript window object. Instead, we create an NPObject of a
    // different class, one which is not bound to a JavaScript object.
    window_script_npobject_ = CreateNoScriptObject();
  }
  VLOG(2) << __PRETTY_FUNCTION__ << ": window_script_npobject="
      << static_cast<void*>(window_script_npobject_);

  return window_script_npobject_;
}

template <typename original_t>
v8::Local<v8::Object> ScriptControllerMixIn<original_t>::CreatePluginWrapper(
    blink::WebPluginContainerImpl* plugin) {
  v8::EscapableHandleScope handle_scope(GetSelf()->GetIsolate());
  v8::Local<v8::Object> scriptable_object =
      plugin->ScriptableObject(GetSelf()->GetIsolate());

  if (scriptable_object.IsEmpty())
    return v8::Local<v8::Object>();

  // LocalFrame Memory Management for NPObjects
  // -------------------------------------
  // NPObjects are treated differently than other objects wrapped by JS.
  // NPObjects can be created either by the browser (e.g. the main
  // window object) or by the plugin (the main plugin object
  // for a HTMLEmbedElement). Further, unlike most DOM Objects, the frame
  // is especially careful to ensure NPObjects terminate at frame teardown because
  // if a plugin leaks a reference, it could leak its objects (or the browser's objects).
  //
  // The LocalFrame maintains a list of plugin objects (m_pluginObjects)
  // which it can use to quickly find the wrapped embed object.
  //
  // Inside the NPRuntime, we've added a few methods for registering
  // wrapped NPObjects. The purpose of the registration is because
  // javascript garbage collection is non-deterministic, yet we need to
  // be able to tear down the plugin objects immediately. When an object
  // is registered, javascript can use it. When the object is destroyed,
  // or when the object's "owning" object is destroyed, the object will
  // be un-registered, and the javascript engine must not use it.
  //
  // Inside the javascript engine, the engine can keep a reference to the
  // NPObject as part of its wrapper. However, before accessing the object
  // it must consult the _NPN_Registry.

  if (blink::IsWrappedNPObject(scriptable_object)) {
    // Track the plugin object. We've been given a reference to the object.
    plugin_objects_.Set(plugin, blink::V8ObjectToNPObject(scriptable_object));
  }

  return handle_scope.Escape(scriptable_object);
}

template class ScriptControllerMixIn<blink::ScriptController>;

}  // namespace neva
}  // namespace blink
