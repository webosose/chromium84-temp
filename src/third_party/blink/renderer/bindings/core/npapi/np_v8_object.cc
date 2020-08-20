/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007, 2008, 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/npapi/np_v8_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_impl.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_priv.h"
#include "third_party/blink/renderer/bindings/core/npapi/v8_np_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include <stdio.h>

namespace blink {

const WrapperTypeInfo* NPObjectTypeInfo() {
  static const WrapperTypeInfo type_info = {
    gin::kEmbedderNPAPI,
    nullptr,
    nullptr,
    "NPObject",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable
  };
  return &type_info;
}

// FIXME: Comments on why use malloc and free.
static NPObject* allocV8NPObject(NPP, NPClass*) {
  return static_cast<NPObject*>(malloc(sizeof(V8NPObject)));
}

static void freeV8NPObject(NPObject* np_object) {
  V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(np_object);
  DisposeUnderlyingV8Object(v8::Isolate::GetCurrent(), np_object);
  free(v8NpObject);
}

static NPClass V8NPObjectClass = {
  NP_CLASS_STRUCT_VERSION,
  allocV8NPObject,
  freeV8NPObject,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static ScriptState* mainWorldScriptState(v8::Isolate* isolate, NPP npp,
    NPObject* np_object) {
  assert(np_object->_class == &V8NPObjectClass);
  V8NPObject* object = reinterpret_cast<V8NPObject*>(np_object);
  LocalDOMWindow* window = object->root_object;
  if (!window || !window->IsCurrentlyDisplayedInFrame())
    return nullptr;
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = ToV8Context(object->root_object->GetFrame(),
                                                DOMWrapperWorld::MainWorld());
  return ScriptState::From(context);
}

static std::unique_ptr<v8::Local<v8::Value>[]> createValueListFromVariantArgs(
    const NPVariant* arguments,
    uint32_t argument_count,
    NPObject* owner,
    v8::Isolate* isolate) {
  std::unique_ptr<v8::Local<v8::Value>[]> argv =
      std::make_unique<v8::Local<v8::Value>[]>(argument_count);
  for (uint32_t index = 0; index < argument_count; index++) {
    const NPVariant* arg = &arguments[index];
    argv[index] = ConvertNPVariantToV8Object(isolate, arg, owner);
  }
  return argv;
}

// Create an identifier (null terminated utf8 char*) from the NPIdentifier.
static v8::Local<v8::String> npIdentifierToV8Identifier(v8::Isolate* isolate,
    NPIdentifier name) {
  PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(name);
  if (identifier->is_string)
    return V8AtomicString(isolate,
                          static_cast<const char*>(identifier->value.string));

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", identifier->value.number);
  return V8AtomicString(isolate, buffer);
}

NPObject* V8ObjectToNPObject(v8::Local<v8::Object> object) {
  return GetInternalField<NPObject, gin::kEncodedValueIndex>(object);
}

bool IsWrappedNPObject(v8::Local<v8::Object> object) {
  return object->InternalFieldCount() == npObjectInternalFieldCount
    && ToWrapperTypeInfo(object) == NPObjectTypeInfo();
}

NPObject* NPCreateV8ScriptObject(v8::Isolate* isolate, NPP npp,
    v8::Local<v8::Object> object, LocalDOMWindow* root) {
  VLOG(2) << __PRETTY_FUNCTION__;

  // Check to see if this object is already wrapped.
  if (IsWrappedNPObject(object)) {
    NPObject* returnValue = V8ObjectToNPObject(object);
    _NPN_RetainObject(returnValue);
    return returnValue;
  }

  V8NPObjectVector* objectVector = nullptr;
  if (V8PerContextData* perContextData =
      V8PerContextData::From(object->CreationContext())) {
    int v8ObjectHash = object->GetIdentityHash();
    assert(v8ObjectHash);
    V8NPObjectMap* v8NPObjectMap = perContextData->GetV8NPObjectMap();
    auto iter = v8NPObjectMap->find(v8ObjectHash);
    if (iter != v8NPObjectMap->end()) {
      V8NPObjectVector& objects = iter->value;
      for (size_t index = 0; index < objects.size(); ++index) {
        V8NPObject* v8np_object = objects.at(index);
        if (v8np_object->v8_object == object &&
            v8np_object->root_object == root) {
          _NPN_RetainObject(&v8np_object->object);
          return reinterpret_cast<NPObject*>(v8np_object);
        }
      }
      objectVector = &iter->value;
    } else {
      objectVector =
          &v8NPObjectMap->Set(v8ObjectHash,
                              V8NPObjectVector()).stored_value->value;
    }
  }
  VLOG(2) << __PRETTY_FUNCTION__;

  V8NPObject* v8np_object =
      reinterpret_cast<V8NPObject*>(_NPN_CreateObject(npp, &V8NPObjectClass));
  // This is uninitialized memory, we need to clear it so that
  // Persistent::Reset won't try to Dispose anything bogus.
  new (&v8np_object->v8_object) v8::Persistent<v8::Object>();
  v8np_object->v8_object.Reset(isolate, object);
  new (&v8np_object->root_object) Persistent<LocalDOMWindow>(root);

  if (objectVector)
    objectVector->push_back(v8np_object);

  return reinterpret_cast<NPObject*>(v8np_object);
}

V8NPObject* NPObjectToV8NPObject(NPObject* np_object) {
  if (np_object->_class != &V8NPObjectClass)
    return nullptr;
  V8NPObject* v8_np_object = reinterpret_cast<V8NPObject*>(np_object);
  if (v8_np_object->v8_object.IsEmpty())
    return nullptr;
  return v8_np_object;
}

void DisposeUnderlyingV8Object(v8::Isolate* isolate, NPObject* np_object) {
  assert(np_object);
  VLOG(2) << __PRETTY_FUNCTION__;

  V8NPObject* v8_np_object = NPObjectToV8NPObject(np_object);
  if (!v8_np_object)
    return;
  VLOG(2) << __PRETTY_FUNCTION__;

  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> v8_object =
      v8::Local<v8::Object>::New(isolate, v8_np_object->v8_object);
  VLOG(2) << __PRETTY_FUNCTION__;

  assert(!v8_object->CreationContext().IsEmpty());
  VLOG(2) << __PRETTY_FUNCTION__;

  if (V8PerContextData* perContextData =
      V8PerContextData::From(v8_object->CreationContext())) {
    VLOG(2) << __PRETTY_FUNCTION__;

    V8NPObjectMap* v8_np_object_map = perContextData->GetV8NPObjectMap();
    int v8_object_hash = v8_object->GetIdentityHash();
    assert(v8_object_hash);
    V8NPObjectMap::iterator iter = v8_np_object_map->find(v8_object_hash);
    if (iter != v8_np_object_map->end()) {
      V8NPObjectVector& objects = iter->value;
      for (size_t index = 0; index < objects.size(); ++index) {
        if (objects.at(index) == v8_np_object) {
          objects.EraseAt(index);
          break;
        }
      }
      if (objects.IsEmpty())
        v8_np_object_map->erase(v8_object_hash);
    }
  }
  v8_np_object->v8_object.Reset();
  v8_np_object->root_object = nullptr;
}

}  // namespace blink

bool _NPN_Invoke(
    NPP npp,
    NPObject* np_object,
    NPIdentifier method_name,
    const NPVariant* arguments,
    uint32_t argument_count,
    NPVariant* result) {
  if (!np_object)
    return false;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  blink::V8NPObject* v8_np_object = blink::NPObjectToV8NPObject(np_object);
  if (!v8_np_object) {
    if (np_object->_class->invoke)
      return np_object->_class->invoke(np_object,
                                       method_name, arguments,
                                       argument_count, result);
    VOID_TO_NPVARIANT(*result);
    return true;
  }

  blink::PrivateIdentifier* identifier =
      static_cast<blink::PrivateIdentifier*>(method_name);
  if (!identifier->is_string)
    return false;

  if (!strcmp(identifier->value.string, "eval")) {
    if (argument_count != 1)
      return false;
    if (arguments[0].type != NPVariantType_String)
      return false;
    return _NPN_Evaluate(npp, np_object,
                         const_cast<NPString*>(&arguments[0].value.stringValue),
                         result);
  }

  // FIXME: should use the plugin's owner frame as the security context.
  blink::ScriptState* script_state =
      blink::mainWorldScriptState(isolate, npp, np_object);
  if (!script_state)
    return false;

  blink::ScriptState::Scope scope(script_state);
  blink::ExceptionCatcher exception_catcher(isolate);

  v8::Local<v8::Object> v8_object =
      v8::Local<v8::Object>::New(isolate, v8_np_object->v8_object);
  v8::Local<v8::Value> function_object =
      v8_object
          ->Get(isolate->GetCurrentContext(),
                blink::V8AtomicString(isolate, identifier->value.string))
          .ToLocalChecked();
  if (function_object.IsEmpty() || function_object->IsNull()) {
    NULL_TO_NPVARIANT(*result);
    return false;
  }
  if (function_object->IsUndefined()) {
    VOID_TO_NPVARIANT(*result);
    return false;
  }

  // Call the function object.
  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(function_object);
  std::unique_ptr<v8::Local<v8::Value>[]> argv =
      blink::createValueListFromVariantArgs(arguments, argument_count,
                                            np_object, isolate);

  v8::Local<v8::Value> result_object;
  if (!blink::V8ScriptRunner::CallFunction(
           function, blink::ExecutionContext::From(script_state), v8_object,
           argument_count, argv.get(), isolate)
           .ToLocal(&result_object))
    return false;

  // If we had an error, return false.  The spec is a little unclear here,
  // but says "Returns true if the method was
  // successfully invoked".
  // If we get an error return value, was that successfully invoked?
  if (result_object.IsEmpty())
    return false;

  blink::ConvertV8ObjectToNPVariant(isolate, result_object, np_object, result);
  return true;
}

// FIXME: Fix it same as _NPN_Invoke (HandleScope and such).
bool _NPN_InvokeDefault(NPP npp, NPObject* np_object,
    const NPVariant* arguments, uint32_t argument_count, NPVariant* result) {
  if (!np_object)
    return false;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  blink::V8NPObject* v8_np_object = blink::NPObjectToV8NPObject(np_object);
  if (!v8_np_object) {
    if (np_object->_class->invokeDefault)
      return np_object->_class->invokeDefault(np_object, arguments,
                                              argument_count, result);

    VOID_TO_NPVARIANT(*result);
    return true;
  }

  VOID_TO_NPVARIANT(*result);

  blink::ScriptState* script_state =
      blink::mainWorldScriptState(isolate, npp, np_object);
  if (!script_state)
    return false;

  blink::ScriptState::Scope scope(script_state);
  blink::ExceptionCatcher exception_catcher(isolate);

  // Lookup the function object and call it.
  v8::Local<v8::Object> function_object =
      v8::Local<v8::Object>::New(isolate, v8_np_object->v8_object);
  if (!function_object->IsFunction())
    return false;

  v8::Local<v8::Value> result_object;
  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(function_object);
  if (!function->IsNull()) {
    std::unique_ptr<v8::Local<v8::Value>[]> argv =
        blink::createValueListFromVariantArgs(arguments, argument_count,
                                              np_object, isolate);
    if (!blink::V8ScriptRunner::CallFunction(
             function, blink::ExecutionContext::From(script_state),
             function_object, argument_count, argv.get(), isolate)
             .ToLocal(&result_object))
      return false;
  }
  // If we had an error, return false.
  // The spec is a little unclear here, but says "Returns true if the method was
  // successfully invoked".  If we get an error return value,
  // was that successfully invoked?
  if (result_object.IsEmpty())
    return false;

  blink::ConvertV8ObjectToNPVariant(isolate, result_object, np_object, result);
  return true;
}

bool _NPN_Evaluate(NPP npp, NPObject* np_object,
    NPString* np_script, NPVariant* result) {
  // FIXME: Give the embedder a way to control this.
  bool popups_allowed = false;
  return _NPN_EvaluateHelper(npp, popups_allowed, np_object, np_script, result);
}

bool _NPN_EvaluateHelper(NPP npp, bool popups_allowed, NPObject* np_object,
    NPString* np_script, NPVariant* result) {
  VOID_TO_NPVARIANT(*result);
  if (blink::ScriptForbiddenScope::IsScriptForbidden())
    return false;

  if (!np_object)
    return false;

  blink::V8NPObject* v8_np_object = blink::NPObjectToV8NPObject(np_object);
  if (!v8_np_object)
    return false;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  blink::ScriptState* script_state =
      blink::mainWorldScriptState(isolate, npp, np_object);
  if (!script_state)
    return false;

  blink::ScriptState::Scope scope(script_state);
  blink::ExceptionCatcher exceptionCatcher(isolate);

  // FIXME: Is this branch still needed after
  // switching to using UserGestureIndicator?
  String filename;
  if (!popups_allowed)
    filename = "npscript";

  blink::LocalFrame* frame = v8_np_object->root_object->GetFrame();
  assert(frame);

  String script = String::FromUTF8(np_script->UTF8Characters,
                                   np_script->UTF8Length);

  v8::Local<v8::Value> v8result =
      frame->GetScriptController().ExecuteScriptAndReturnValue(
          script_state->GetContext(), blink::ScriptSourceCode(script),
          blink::KURL(), blink::SanitizeScriptErrors::kSanitize);

  if (v8result.IsEmpty())
    return false;

  if (_NPN_IsAlive(np_object))
    blink::ConvertV8ObjectToNPVariant(isolate, v8result, np_object, result);
  return true;
}

bool _NPN_GetProperty(NPP npp, NPObject* np_object,
    NPIdentifier property_name, NPVariant* result) {
  if (!np_object)
    return false;

  if (blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object)) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    blink::ScriptState* script_state =
        blink::mainWorldScriptState(isolate, npp, np_object);
    if (!script_state)
      return false;

    blink::ScriptState::Scope scope(script_state);
    blink::ExceptionCatcher exceptionCatcher(isolate);

    v8::Local<v8::Object> obj =
        v8::Local<v8::Object>::New(isolate, object->v8_object);
    v8::Local<v8::Value> v8result =
        obj->Get(isolate->GetCurrentContext(),
                 blink::npIdentifierToV8Identifier(isolate, property_name))
            .ToLocalChecked();

    if (v8result.IsEmpty())
      return false;

    blink::ConvertV8ObjectToNPVariant(isolate, v8result, np_object, result);
    return true;
  }

  if (np_object->_class->hasProperty && np_object->_class->getProperty) {
    if (np_object->_class->hasProperty(np_object, property_name))
      return np_object->_class->getProperty(np_object, property_name, result);
  }

  VOID_TO_NPVARIANT(*result);
  return false;
}

bool _NPN_SetProperty(NPP npp, NPObject* np_object,
    NPIdentifier property_name, const NPVariant* value) {
  if (!np_object)
    return false;

  if (blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object)) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    blink::ScriptState* script_state =
        blink::mainWorldScriptState(isolate, npp, np_object);
    if (!script_state)
      return false;

    blink::ScriptState::Scope scope(script_state);
    blink::ExceptionCatcher exception_catcher(isolate);

    v8::Local<v8::Object> obj =
        v8::Local<v8::Object>::New(isolate, object->v8_object);
    // GetGetFrame()->GetScriptController().windowScriptNPObject(); ?
    obj->Set(isolate->GetCurrentContext(),
             blink::npIdentifierToV8Identifier(isolate, property_name),
             blink::ConvertNPVariantToV8Object(isolate, value,
                                               object->root_object->GetFrame()
                                                   ->GetScriptController()
                                                   .GetWindowScriptNPObject()))
        .Check();
    return true;
  }

  if (np_object->_class->setProperty)
    return np_object->_class->setProperty(np_object, property_name, value);

  return false;
}

bool _NPN_RemoveProperty(NPP npp, NPObject* np_object,
    NPIdentifier property_name) {
  if (!np_object)
    return false;

  blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object);
  if (!object)
    return false;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  blink::ScriptState* script_state =
      blink::mainWorldScriptState(isolate, npp, np_object);
  if (!script_state)
    return false;
  blink::ScriptState::Scope scope(script_state);
  blink::ExceptionCatcher exception_catcher(isolate);

  v8::Local<v8::Object> obj =
      v8::Local<v8::Object>::New(isolate, object->v8_object);
  // FIXME: Verify that setting to undefined is right.
  obj->Set(isolate->GetCurrentContext(),
           blink::npIdentifierToV8Identifier(isolate, property_name),
           v8::Undefined(isolate))
      .Check();
  return true;
}

bool _NPN_HasProperty(NPP npp, NPObject* np_object,
    NPIdentifier property_name) {
  if (!np_object)
    return false;

  if (blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object)) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    blink::ScriptState* script_state =
        blink::mainWorldScriptState(isolate, npp, np_object);
    if (!script_state)
      return false;
    blink::ScriptState::Scope scope(script_state);
    blink::ExceptionCatcher exception_catcher(isolate);

    v8::Local<v8::Object> obj =
        v8::Local<v8::Object>::New(isolate, object->v8_object);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    bool result;
    return obj->Has(context,
                    blink::npIdentifierToV8Identifier(isolate, property_name))
               .To(&result) &&
           result;
  }

  if (np_object->_class->hasProperty)
    return np_object->_class->hasProperty(np_object, property_name);
  return false;
}

bool _NPN_HasMethod(NPP npp, NPObject* np_object, NPIdentifier method_name) {
  if (!np_object)
    return false;

  if (blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object)) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    blink::ScriptState* scriptState =
        blink::mainWorldScriptState(isolate, npp, np_object);
    if (!scriptState)
      return false;
    blink::ScriptState::Scope scope(scriptState);
    blink::ExceptionCatcher exceptionCatcher(isolate);

    v8::Local<v8::Object> obj =
        v8::Local<v8::Object>::New(isolate, object->v8_object);
    v8::Local<v8::Value> prop =
        obj->Get(isolate->GetCurrentContext(),
                 blink::npIdentifierToV8Identifier(isolate, method_name))
            .ToLocalChecked();
    return prop->IsFunction();
  }

  if (np_object->_class->hasMethod)
    return np_object->_class->hasMethod(np_object, method_name);
  return false;
}

void _NPN_SetException(NPObject* np_object, const NPUTF8 *message) {
  if (!np_object || !blink::NPObjectToV8NPObject(np_object)) {
    // We won't be able to find a proper scope for this exception, so just throw it.
    // This is consistent with JSC, which throws a global exception all the time.
    blink::V8ThrowException::ThrowError(v8::Isolate::GetCurrent(), message);
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  blink::ScriptState* script_state =
      blink::mainWorldScriptState(isolate, 0, np_object);
  if (!script_state)
    return;

  blink::ScriptState::Scope scope(script_state);
  blink::ExceptionCatcher exception_catcher(isolate);

  blink::V8ThrowException::ThrowError(isolate, message);
}

bool _NPN_Enumerate(NPP npp, NPObject* np_object,
    NPIdentifier** identifier, uint32_t* count) {
  if (!np_object)
    return false;

  if (blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object)) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    blink::ScriptState* script_state =
        blink::mainWorldScriptState(isolate, npp, np_object);
    if (!script_state)
      return false;
    blink::ScriptState::Scope scope(script_state);
    blink::ExceptionCatcher exception_catcher(isolate);

    v8::Local<v8::Object> obj =
        v8::Local<v8::Object>::New(isolate, object->v8_object);

    // FIXME: http://b/issue?id=1210340: Use a v8::Object::Keys() method when it exists, instead of evaluating javascript.

    // FIXME: Figure out how to cache this helper function.  Run a helper function that collects the properties
    // on the object into an array.
    String source =
        "(function (obj) {"
        "  var props = [];"
        "  for (var prop in obj) {"
        "    props[props.length] = prop;"
        "  }"
        "  return props;"
        "});";
    v8::Local<v8::Value> result;
    if (!blink::V8ScriptRunner::CompileAndRunInternalScript(
             isolate, script_state,
             blink::ScriptSourceCode(source,
                                     blink::ScriptSourceLocationType::kInternal,
                                     nullptr, blink::KURL(), TextPosition()))
             .ToLocal(&result))
      return false;
    assert(result->IsFunction());
    v8::Local<v8::Function> enumerator = v8::Local<v8::Function>::Cast(result);
    v8::Local<v8::Value> argv[] = { obj };
    v8::Local<v8::Value> props_obj;
    // FIXME(neva): CallFunction replaced CallInternalFunction which is deprecated
    // in upstream (https://chromium-review.googlesource.com/c/chromium/src/+/1913467)
    // Check and revise if needed
    if (!blink::V8ScriptRunner::CallFunction(
              enumerator, blink::ExecutionContext::From(script_state),
              v8::Local<v8::Object>::Cast(result), base::size(argv), argv, isolate)
             .ToLocal(&props_obj))
      return false;

    // Convert the results into an array of NPIdentifiers.
    v8::Local<v8::Array> props = v8::Local<v8::Array>::Cast(props_obj);
    *count = props->Length();
    *identifier =
        static_cast<NPIdentifier*>(calloc(*count, sizeof(NPIdentifier)));
    for (uint32_t i = 0; i < *count; ++i) {
      v8::Local<v8::Value> name =
          props->Get(isolate->GetCurrentContext(), v8::Integer::New(isolate, i))
              .ToLocalChecked();
      (*identifier)[i] = blink::GetStringIdentifier(
          v8::Local<v8::String>::Cast(name), isolate);
    }
    return true;
  }

  if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(np_object->_class) &&
      np_object->_class->enumerate)
    return np_object->_class->enumerate(np_object, identifier, count);

  return false;
}

bool _NPN_Construct(NPP npp, NPObject* np_object, const NPVariant* arguments,
    uint32_t argument_count, NPVariant* result) {
  if (!np_object)
    return false;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  if (blink::V8NPObject* object = blink::NPObjectToV8NPObject(np_object)) {
    blink::ScriptState* script_state =
        blink::mainWorldScriptState(isolate, npp, np_object);
    if (!script_state)
      return false;
    blink::ScriptState::Scope scope(script_state);
    blink::ExceptionCatcher exception_catcher(isolate);

    // Lookup the constructor function.
    v8::Local<v8::Object> ctor_obj =
        v8::Local<v8::Object>::New(isolate, object->v8_object);
    if (!ctor_obj->IsFunction())
      return false;

    // Call the constructor.
    v8::Local<v8::Object> result_object;
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(ctor_obj);
    if (!ctor->IsNull()) {
      std::unique_ptr<v8::Local<v8::Value>[]> argv =
          blink::createValueListFromVariantArgs(arguments, argument_count,
                                                np_object, isolate);
      if (!blink::V8ObjectConstructor::NewInstance(isolate, ctor,
                                                   argument_count, argv.get())
               .ToLocal(&result_object))
        return false;
    }

    if (result_object.IsEmpty())
      return false;

    blink::ConvertV8ObjectToNPVariant(isolate, result_object, np_object,
                                      result);
    return true;
  }

  if (NP_CLASS_STRUCT_VERSION_HAS_CTOR(np_object->_class)
      && np_object->_class->construct)
    return np_object->_class->construct(np_object, arguments,
                                        argument_count, result);

  return false;
}
