/* Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_V8_NP_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_V8_NP_UTILS_H_

#include "neva/npapi/bindings/npruntime.h"
#include "third_party/blink/renderer/core/core_export.h"
#include <v8.h>

namespace blink {

// Convert a V8 Value of any type (string, bool, object, etc) to a NPVariant.
CORE_EXPORT void ConvertV8ObjectToNPVariant(v8::Isolate*, v8::Local<v8::Value>,
    NPObject*, NPVariant*);

// Convert a NPVariant (string, bool, object, etc) back to a V8 Value.
// The owner object is the NPObject which relates to the
// object, if the object is an Object.
// The created NPObject will be tied to the lifetime of the owner.
CORE_EXPORT v8::Handle<v8::Value> ConvertNPVariantToV8Object(v8::Isolate*,
    const NPVariant*, NPObject*);

// Helper function to create an NPN String Identifier from a v8 string.
NPIdentifier GetStringIdentifier(v8::Handle<v8::String>, v8::Isolate* isolate);

// The ExceptionHandler will be notified of any exceptions thrown while
// operating on a NPObject.
typedef void (*ExceptionHandler)(void* data, const NPUTF8* message);
void PushExceptionHandler(ExceptionHandler, void* data);
void PopExceptionHandler();

// Upon destruction, an ExceptionCatcher will pass a caught exception to the
// current ExceptionHandler.
class ExceptionCatcher {
 public:
  ExceptionCatcher(v8::Isolate* isolate);
  ~ExceptionCatcher();
 private:
  v8::TryCatch try_catch_;
  v8::Isolate* isolate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_V8_NP_UTILS_H_
