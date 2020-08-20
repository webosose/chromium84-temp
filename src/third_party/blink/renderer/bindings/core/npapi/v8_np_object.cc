/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/npapi/np_v8_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_impl.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_priv.h"
#include "third_party/blink/renderer/bindings/core/npapi/v8_np_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/v8_np_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_embed_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_object_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_map.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_global_value_map.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "v8-util.h"

namespace blink {

enum InvokeFunctionType {
  InvokeMethod = 1,
  InvokeConstruct = 2,
  InvokeDefault = 3
};

struct IdentifierRep {
  int Number() const { return is_string_ ? 0 : value_.number_; }
  const char* String() const { return is_string_ ? value_.string_ : 0; }

  union {
    const char* string_;
    int number_;
  } value_;
  bool is_string_;
};

// FIXME: need comments.
// Params: holder could be HTMLEmbedElement or NPObject
static void NPObjectInvokeImpl(const v8::FunctionCallbackInfo<v8::Value>& info,
    InvokeFunctionType function_id) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  NPObject* np_object;
  v8::Isolate* isolate = info.GetIsolate();

  HTMLPlugInElement* element = nullptr;
  if (!element) {
    element = V8HTMLEmbedElement::ToImplWithTypeCheck(isolate, info.Holder());
    if (!element) {
      element = V8HTMLObjectElement::ToImplWithTypeCheck(isolate, info.Holder());
    }
  }
  if (element) {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Object> wrapper = element->PluginWrapper();
    np_object = (!wrapper.IsEmpty()) ? V8ObjectToNPObject(wrapper) : nullptr;
  } else {
    // The holder object is not a subtype of HTMLPlugInElement,
    // it must be an NPObject which has three internal fields.
    if (info.Holder()->InternalFieldCount() != npObjectInternalFieldCount) {
      V8ThrowException::ThrowReferenceError(info.GetIsolate(),
                                            "NPMethod called on non-NPObject");
      return;
    }

    np_object = V8ObjectToNPObject(info.Holder());
  }

  // Verify that our wrapper wasn't using a NPObject which has already been deleted.
  if (!np_object || !_NPN_IsAlive(np_object)) {
    V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
    return;
  }

  // Wrap up parameters.
  int num_args = info.Length();
  std::unique_ptr<NPVariant[]> np_args = std::make_unique<NPVariant[]>(num_args);

  for (int i = 0; i < num_args; i++)
    ConvertV8ObjectToNPVariant(isolate, info[i], np_object, &np_args[i]);

  NPVariant result;
  VOID_TO_NPVARIANT(result);

  bool retval = true;
  switch (function_id) {
  case InvokeMethod:
    if (np_object->_class->invoke) {
      v8::Handle<v8::String> functionName =
          v8::Handle<v8::String>::Cast(info.Data());
      NPIdentifier identifier = GetStringIdentifier(functionName, isolate);
      retval = np_object->_class->invoke(np_object, identifier,
                                          np_args.get(), num_args, &result);
    }
    break;
  case InvokeConstruct:
    if (np_object->_class->construct)
      retval = np_object->_class->construct(np_object, np_args.get(),
                                            num_args, &result);
    break;
  case InvokeDefault:
    if (np_object->_class->invokeDefault)
      retval = np_object->_class->invokeDefault(np_object, np_args.get(),
                                                num_args, &result);
    break;
  default:
    break;
  }

  if (!retval)
    V8ThrowException::ThrowError(isolate, "Error calling method on NPObject.");

  for (int i = 0; i < num_args; i++)
    _NPN_ReleaseVariantValue(&np_args[i]);

  // Unwrap return values.
  v8::Handle<v8::Value> return_value;
  if (_NPN_IsAlive(np_object))
    return_value = ConvertNPVariantToV8Object(isolate, &result, np_object);
  _NPN_ReleaseVariantValue(&result);

  V8SetReturnValue(info, return_value);
}

static void NPObjectMethodHandler(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return NPObjectInvokeImpl(info, InvokeMethod);
}

static void NPObjectInvokeDefaultHandler(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.IsConstructCall()) {
    NPObjectInvokeImpl(info, InvokeConstruct);
    return;
  }

  NPObjectInvokeImpl(info, InvokeDefault);
}

class V8TemplateMapTraits :
    public V8GlobalValueMapTraits<PrivateIdentifier*,
                                  v8::FunctionTemplate,
                                  v8::kWeakWithParameter> {
public:
  typedef v8::GlobalValueMap<PrivateIdentifier*,  v8::FunctionTemplate,
                              V8TemplateMapTraits> MapType;
  typedef PrivateIdentifier WeakCallbackDataType;

  static WeakCallbackDataType* WeakCallbackParameter(
      MapType* map, PrivateIdentifier* key,
      const v8::Local<v8::FunctionTemplate>& value) {
    return key;
  }

  static void DisposeCallbackData(WeakCallbackDataType* callbackData) { }

  static MapType* MapFromWeakCallbackInfo(
    const v8::WeakCallbackInfo<WeakCallbackDataType>&);

  static PrivateIdentifier* KeyFromWeakCallbackInfo(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
    return data.GetParameter();
  }

  // Dispose traits:
  static void OnWeakCallback(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) { }
  static void Dispose(
      v8::Isolate* isolate, v8::Global<v8::FunctionTemplate> value,
      PrivateIdentifier* key) { }
  static void DisposeWeak(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) { }
};

class V8NPTemplateMap {
public:
  // NPIdentifier is PrivateIdentifier*.
  typedef v8::GlobalValueMap<PrivateIdentifier*,
                              v8::FunctionTemplate,
                              V8TemplateMapTraits> MapType;

  v8::Local<v8::FunctionTemplate> get(PrivateIdentifier* key) {
    return m_map.Get(key);
  }

  void set(PrivateIdentifier* key, v8::Handle<v8::FunctionTemplate> handle) {
    assert(!m_map.Contains(key));
    m_map.Set(key, handle);
  }

  static V8NPTemplateMap& sharedInstance(v8::Isolate* isolate) {
    DEFINE_STATIC_LOCAL(V8NPTemplateMap, map, (isolate));
    assert(isolate == map.m_map.GetIsolate());
    return map;
  }

  friend class V8TemplateMapTraits;

private:
  explicit V8NPTemplateMap(v8::Isolate* isolate)
    : m_map(isolate) {
  }

  MapType m_map;
};

V8TemplateMapTraits::MapType* V8TemplateMapTraits::MapFromWeakCallbackInfo(
  const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  return &V8NPTemplateMap::sharedInstance(data.GetIsolate()).m_map;
}

static v8::Handle<v8::Value> npObjectGetProperty(v8::Local<v8::Object> self,
  NPIdentifier identifier, v8::Local<v8::Value> key, v8::Isolate* isolate) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  NPObject* npObject = V8ObjectToNPObject(self);

  // Verify that our wrapper wasn't using a NPObject which
  // has already been deleted.
  if (!npObject || !_NPN_IsAlive(npObject)) {
    V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
    return v8::Undefined(isolate);
  }

  if (npObject->_class->hasProperty && npObject->_class->getProperty &&
      npObject->_class->hasProperty(npObject, identifier)) {
    if (!_NPN_IsAlive(npObject)) {
      V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
      return v8::Undefined(isolate);
    }

    NPVariant result;
    VOID_TO_NPVARIANT(result);
    if (!npObject->_class->getProperty(npObject, identifier, &result))
      return v8::Local<v8::Value>();

    v8::Handle<v8::Value> returnValue;
    if (_NPN_IsAlive(npObject))
      returnValue = ConvertNPVariantToV8Object(isolate, &result, npObject);
    _NPN_ReleaseVariantValue(&result);
    return returnValue;
  }

  if (!_NPN_IsAlive(npObject)) {
    V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
    return v8::Undefined(isolate);
  }

  if (key->IsString() && npObject->_class->hasMethod &&
      npObject->_class->hasMethod(npObject, identifier)) {
    if (!_NPN_IsAlive(npObject)) {
      V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
      return v8::Undefined(isolate);
    }

    PrivateIdentifier* id = static_cast<PrivateIdentifier*>(identifier);
    v8::Local<v8::FunctionTemplate> functionTemplate =
        V8NPTemplateMap::sharedInstance(isolate).get(id);
    // Cache templates using identifier as the key.
    if (functionTemplate.IsEmpty()) {
      // Create a new template.
      functionTemplate = v8::FunctionTemplate::New(isolate);
      functionTemplate->SetCallHandler(NPObjectMethodHandler, key);
      V8NPTemplateMap::sharedInstance(isolate).set(id, functionTemplate);
    }
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Function> v8Function =
        functionTemplate->GetFunction(context).ToLocalChecked();
    v8Function->SetName(v8::Handle<v8::String>::Cast(key));
    return v8Function;
  }

  return v8::Local<v8::Value>();
}

static void NPObjectNamedPropertyGetter(v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DCHECK(name->IsString());
  v8::Local<v8::String> strName =
      name->ToString(info.GetIsolate()->GetCurrentContext()).ToLocalChecked();
  NPIdentifier identifier = GetStringIdentifier(strName, info.GetIsolate());
  V8SetReturnValue(info, npObjectGetProperty(info.Holder(),
                    identifier, strName, info.GetIsolate()));
}

static void NPObjectIndexedPropertyGetter(uint32_t index,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  NPIdentifier identifier = _NPN_GetIntIdentifier(index);
  V8SetReturnValue(info, npObjectGetProperty(info.Holder(), identifier,
                    v8::Number::New(info.GetIsolate(), index),
                    info.GetIsolate()));
}

static void NPObjectQueryProperty(v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Integer>& info) {
  if (!name->IsString())
    return;
  v8::Local<v8::String> strName =
      name->ToString(info.GetIsolate()->GetCurrentContext()).ToLocalChecked();
  NPIdentifier identifier = GetStringIdentifier(strName, info.GetIsolate());
  if (npObjectGetProperty(info.Holder(), identifier, strName,
      info.GetIsolate()).IsEmpty())
    return;
  V8SetReturnValueInt(info, 0);
}

static v8::Handle<v8::Value> NPObjectSetProperty(v8::Local<v8::Object> self,
    NPIdentifier identifier, v8::Local<v8::Value> value, v8::Isolate* isolate) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  NPObject* npObject = V8ObjectToNPObject(self);

  // Verify that our wrapper wasn't using a NPObject which has already been deleted.
  if (!npObject || !_NPN_IsAlive(npObject)) {
    V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
    return value; // Intercepted, but an exception was thrown.
  }

  if (npObject->_class->hasProperty && npObject->_class->setProperty &&
      npObject->_class->hasProperty(npObject, identifier)) {
    if (!_NPN_IsAlive(npObject)) {
      V8ThrowException::ThrowReferenceError(isolate, "NPObject deleted");
      return v8::Undefined(isolate);
    }

    NPVariant npValue;
    VOID_TO_NPVARIANT(npValue);
    ConvertV8ObjectToNPVariant(isolate, value, npObject, &npValue);
    bool success = npObject->_class->setProperty(npObject, identifier, &npValue);
    _NPN_ReleaseVariantValue(&npValue);
    if (success)
      return value; // Intercept the call.
  }
  return v8::Local<v8::Value>();
}

static void NPObjectNamedPropertySetter(v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DCHECK(name->IsString());
  v8::Local<v8::String> strName =
      name->ToString(info.GetIsolate()->GetCurrentContext()).ToLocalChecked();
  NPIdentifier identifier = GetStringIdentifier(strName, info.GetIsolate());
  V8SetReturnValue(info, NPObjectSetProperty(info.Holder(),
                    identifier, value, info.GetIsolate()));
}

static void NPObjectIndexedPropertySetter(uint32_t index,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  NPIdentifier identifier = _NPN_GetIntIdentifier(index);
  V8SetReturnValue(info, NPObjectSetProperty(info.Holder(),
                    identifier, value, info.GetIsolate()));
}

static void NPObjectPropertyEnumerator(
    const v8::PropertyCallbackInfo<v8::Array>& info,
    bool namedProperty) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  NPObject* np_object = V8ObjectToNPObject(info.Holder());

  // Verify that our wrapper wasn't using a NPObject which
  // has already been deleted.
  if (!np_object || !_NPN_IsAlive(np_object)) {
    V8ThrowException::ThrowReferenceError(info.GetIsolate(), "NPObject deleted");
    return;
  }

  if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(np_object->_class) &&
      np_object->_class->enumerate) {
    uint32_t count = 0;
    NPIdentifier* identifiers = nullptr;
    if (np_object->_class->enumerate(np_object, &identifiers, &count)) {
      uint32_t propertiesCount = 0;
      for (uint32_t i = 0; i < count; ++i) {
        IdentifierRep* identifier = static_cast<IdentifierRep*>(identifiers[i]);
        if (namedProperty == identifier->is_string_)
          ++propertiesCount;
      }
      v8::Handle<v8::Array> properties =
        v8::Array::New(info.GetIsolate(), propertiesCount);
      for (uint32_t i = 0, propertyIndex = 0; i < count; ++i) {
        IdentifierRep* identifier = static_cast<IdentifierRep*>(identifiers[i]);
        if (namedProperty == identifier->is_string_) {
          assert(propertyIndex < propertiesCount);
          if (namedProperty)
            ALLOW_UNUSED_LOCAL(properties->Set(
                info.GetIsolate()->GetCurrentContext(),
                v8::Integer::New(info.GetIsolate(), propertyIndex++),
                V8AtomicString(info.GetIsolate(), identifier->String())));
          else
            ALLOW_UNUSED_LOCAL(properties->Set(
                info.GetIsolate()->GetCurrentContext(),
                v8::Integer::New(info.GetIsolate(), propertyIndex++),
                v8::Integer::New(info.GetIsolate(), identifier->Number())));
        }
      }

      V8SetReturnValue(info, properties);
      return;
    }
  }
}

static void NPObjectNamedPropertyEnumerator(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  NPObjectPropertyEnumerator(info, true);
}

static void NPObjectIndexedPropertyEnumerator(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  NPObjectPropertyEnumerator(info, false);
}

static DOMWrapperMap<NPObject>& StaticNPObjectMap() {
  DEFINE_STATIC_LOCAL(DOMWrapperMap<NPObject>, np_object_map, (v8::Isolate::GetCurrent()));
  return np_object_map;
}

template <>
inline void DOMWrapperMap<NPObject>::PersistentValueMapTraits::Dispose(
  v8::Isolate* isolate,
  v8::UniquePersistent<v8::Object> value,
  NPObject* np_object) {
  assert(np_object);
  if (_NPN_IsAlive(np_object))
    _NPN_ReleaseObject(np_object);
}

template <>
inline void DOMWrapperMap<NPObject>::PersistentValueMapTraits::DisposeWeak(
    const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
  NPObject* np_object = KeyFromWeakCallbackInfo(data);
  assert(np_object);
  if (_NPN_IsAlive(np_object))
    _NPN_ReleaseObject(np_object);
}

v8::Local<v8::Object> CreateV8ObjectForNPObject(v8::Isolate* isolate,
    NPObject* object, NPObject* root) {
  static v8::Eternal<v8::FunctionTemplate> np_object_desc;

  assert(isolate->InContext());

  // If this is a v8 object, just return it.
  V8NPObject* v8_np_object = NPObjectToV8NPObject(object);
  if (v8_np_object)
    return v8::Local<v8::Object>::New(isolate, v8_np_object->v8_object);

  // If we've already wrapped this object, just return it.
  v8::Handle<v8::Object> wrapper = StaticNPObjectMap().NewLocal(isolate, object);
  if (!wrapper.IsEmpty())
    return wrapper;

  // FIXME: we should create a Wrapper type as a subclass of JSObject.
  // It has two internal fields, field 0 is the wrapped
  // pointer, and field 1 is the type. There should be an api function
  // that returns unused type id. The same Wrapper type
  // can be used by DOM bindings.
  if (np_object_desc.IsEmpty()) {
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);

    templ->InstanceTemplate()->SetInternalFieldCount(npObjectInternalFieldCount);
    v8::NamedPropertyHandlerConfiguration configuration(
      NPObjectNamedPropertyGetter,
      NPObjectNamedPropertySetter,
      NPObjectQueryProperty,
      nullptr,
      NPObjectNamedPropertyEnumerator,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kOnlyInterceptStrings);

    templ->InstanceTemplate()->SetHandler(configuration);
    templ->InstanceTemplate()
        ->SetIndexedPropertyHandler(NPObjectIndexedPropertyGetter,
                                    NPObjectIndexedPropertySetter, 0, 0,
                                    NPObjectIndexedPropertyEnumerator);
    templ->InstanceTemplate()
        ->SetCallAsFunctionHandler(NPObjectInvokeDefaultHandler);
    np_object_desc.Set(isolate, templ);
  }

  // FIXME: Move StaticNPObjectMap() to DOMDataStore.
  // Use V8DOMWrapper::createWrapper() and
  // V8DOMWrapper::associateObjectWithWrapper()
  // to create a wrapper object.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Function> v8_function =
      np_object_desc.Get(isolate)->GetFunction(context).ToLocalChecked();
  v8::Local<v8::Object> value;
  if (!V8ObjectConstructor::NewInstance(isolate, v8_function).ToLocal(&value))
    return value;

  int indices[] = {gin::kEncodedValueIndex, gin::kWrapperInfoIndex};
  void* values[] = {object, const_cast<WrapperTypeInfo*>(NPObjectTypeInfo())};
  value->SetAlignedPointerInInternalFields(base::size(indices), indices,
                                           values);

  // KJS retains the object as part of its wrapper (see Bindings::CInstance).
  _NPN_RetainObject(object);
  _NPN_RegisterObject(object, root);

  (void)StaticNPObjectMap().Set(object, NPObjectTypeInfo(), value);
  return value;
}

void ForgetV8ObjectForNPObject(NPObject* object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::Object> wrapper = StaticNPObjectMap().NewLocal(isolate, object);
  if (!wrapper.IsEmpty()) {
    int indices[] = {gin::kEncodedValueIndex, gin::kWrapperInfoIndex};
    void* values[] = {nullptr, nullptr};
    wrapper->SetAlignedPointerInInternalFields(base::size(indices), indices,
                                               values);
    StaticNPObjectMap().RemoveAndDispose(object);
    _NPN_ReleaseObject(object);
  }
}

}  // namespace blink
