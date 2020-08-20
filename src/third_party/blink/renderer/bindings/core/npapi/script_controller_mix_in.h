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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_SCRIPT_CONTROLLER_MIX_IN_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_SCRIPT_CONTROLLER_MIX_IN_H_

#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "v8/include/v8.h"

struct NPObject;

namespace WTF {

class String;

// In order not to include npruntime_impl.h everywhere we use this
// specialization of template IsPointerToGarbageCollectedType for
// pointer to NPObject.
template <>
class IsPointerToGarbageCollectedType<NPObject*, false> {
 public:
  static const bool value = false;
};

}

namespace blink {

class WebPluginContainerImpl;
class HTMLPlugInElement;
class LocalFrame;

namespace neva {

template <typename original_t>
class ScriptControllerMixIn
    : public GarbageCollected<ScriptControllerMixIn<original_t> > {
 protected:
  const original_t* GetSelf();
  typedef HeapHashMap<Member<WebPluginContainerImpl>,
      NPObject*> PluginObjectMap;
  PluginObjectMap plugin_objects_;
  NPObject* window_script_npobject_;

 public:
  virtual void Trace(Visitor*);
  bool BindToWindowObject(LocalFrame*, const WTF::String& key, NPObject*);
  void ClearScriptObjects();
  void CleanupScriptObjectsForPlugin(WebPluginContainerImpl*);
  NPObject* CreateScriptObjectForPluginElement(HTMLPlugInElement*);
  NPObject* GetWindowScriptNPObject();
  v8::Local<v8::Object> CreatePluginWrapper(WebPluginContainerImpl*);
};

}  // namespace neva
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_SCRIPT_CONTROLLER_MIX_IN_H_
