/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <stdlib.h>

namespace blink {

void ConvertV8ObjectToNPVariant(v8::Isolate* isolate,
    v8::Local<v8::Value> object, NPObject* owner, NPVariant* result) {
  VOID_TO_NPVARIANT(*result);

  // It is really the caller's responsibility to deal with the empty handle case because there could be different actions to
  // take in different contexts.
  assert(!object.IsEmpty());

  if (object.IsEmpty())
    return;

  if (object->IsNumber()) {
    DOUBLE_TO_NPVARIANT(
        object->NumberValue(isolate->GetCurrentContext()).ToChecked(), *result);
  } else if (object->IsBoolean()) {
    BOOLEAN_TO_NPVARIANT(
        object->BooleanValue(isolate), *result);
  } else if (object->IsNull()) {
    NULL_TO_NPVARIANT(*result);
  } else if (object->IsUndefined()) {
    VOID_TO_NPVARIANT(*result);
  } else if (object->IsString()) {
    v8::Handle<v8::String> v8str = object.As<v8::String>();
    int length = v8str->Utf8Length(isolate) + 1;
    char* utf8_chars = reinterpret_cast<char*>(malloc(length));
    v8str->WriteUtf8(isolate, utf8_chars, length, 0,
                     v8::String::HINT_MANY_WRITES_EXPECTED);
    STRINGN_TO_NPVARIANT(utf8_chars, length-1, *result);
  } else if (object->IsObject()) {
    LocalDOMWindow* window = CurrentDOMWindow(isolate);
    NPObject* npobject =
        NPCreateV8ScriptObject(isolate, 0,
                                v8::Handle<v8::Object>::Cast(object), window);
    if (npobject)
      _NPN_RegisterObject(npobject, owner);
    OBJECT_TO_NPVARIANT(npobject, *result);
  }
}

v8::Handle<v8::Value> ConvertNPVariantToV8Object(v8::Isolate* isolate,
    const NPVariant* variant, NPObject* owner) {
  NPVariantType type = variant->type;

  switch (type) {
  case NPVariantType_Int32:
    return v8::Integer::New(isolate, NPVARIANT_TO_INT32(*variant));
  case NPVariantType_Double:
    return v8::Number::New(isolate, NPVARIANT_TO_DOUBLE(*variant));
  case NPVariantType_Bool:
    return V8Boolean(NPVARIANT_TO_BOOLEAN(*variant), isolate);
  case NPVariantType_Null:
    return v8::Null(isolate);
  case NPVariantType_Void:
    return v8::Undefined(isolate);
  case NPVariantType_String: {
    NPString src = NPVARIANT_TO_STRING(*variant);
    return V8AtomicStringFromUtf8(isolate, src.UTF8Characters, src.UTF8Length);
  }
  case NPVariantType_Object: {
    NPObject* object = NPVARIANT_TO_OBJECT(*variant);
    if (V8NPObject* v8_np_object = NPObjectToV8NPObject(object))
      return v8::Local<v8::Object>::New(isolate, v8_np_object->v8_object);
    return CreateV8ObjectForNPObject(isolate, object, owner);
  }
  default:
    return v8::Undefined(isolate);
  }
}

// Helper function to create an NPN String Identifier from a v8 string.
NPIdentifier GetStringIdentifier(v8::Handle<v8::String> str,
    v8::Isolate* isolate) {
  const int k_stack_buffer_size = 100;

  int buffer_length = str->Utf8Length(isolate) + 1;
  if (buffer_length <= k_stack_buffer_size) {
    // Use local stack buffer to avoid heap allocations for small strings.
    // Here we should only use the stack space for
    // stackBuffer when it's used, not when we use the heap.
    //
    // WriteUtf8 is guaranteed to generate a null-terminated string
    // because bufferLength is constructed to be one greater
    // than the string length.
    char stack_buffer[k_stack_buffer_size];
    str->WriteUtf8(isolate, stack_buffer, buffer_length);
    return _NPN_GetStringIdentifier(stack_buffer);
  }

  v8::String::Utf8Value utf8(isolate, str);
  return _NPN_GetStringIdentifier(*utf8);
}

struct ExceptionHandlerInfo {
  ExceptionHandlerInfo* previous;
  ExceptionHandler handler;
  void* data;
};

static ExceptionHandlerInfo* top_handler;

void PushExceptionHandler(ExceptionHandler handler, void* data) {
  ExceptionHandlerInfo* info = new ExceptionHandlerInfo;
  info->previous = top_handler;
  info->handler = handler;
  info->data = data;
  top_handler = info;
}

void PopExceptionHandler() {
  assert(top_handler);
  ExceptionHandlerInfo* doomed = top_handler;
  top_handler = top_handler->previous;
  delete doomed;
}

ExceptionCatcher::ExceptionCatcher(v8::Isolate* isolate) :
    try_catch_(isolate),
    isolate_(isolate) {
  if (!top_handler)
    try_catch_.SetVerbose(true);
}

ExceptionCatcher::~ExceptionCatcher() {
  if (!try_catch_.HasCaught())
    return;

  if (top_handler)
    top_handler->handler(top_handler->data,
                          *v8::String::Utf8Value(isolate_,
                                                  try_catch_.Exception()));
}

}  // namespace blink
