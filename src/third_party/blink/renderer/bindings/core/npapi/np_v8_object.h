/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_NP_V8_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_NP_V8_OBJECT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

// Chromium uses npruntime.h from the Chromium source repository under
// third_party/npapi/bindings.
#include "neva/npapi/bindings/npruntime.h"
#include <v8.h>

namespace blink {

static const int npObjectInternalFieldCount = gin::kNumberOfInternalFields + 0;

const WrapperTypeInfo* NPObjectTypeInfo();

// A V8NPObject is a NPObject which carries additional V8-specific information.
// It is created with NPCreateV8ScriptObject() and deallocated via the
// deallocate method in the same way as other NPObjects.
struct V8NPObject {
public:
  NPObject object;
  v8::Persistent<v8::Object> v8_object;
  Persistent<LocalDOMWindow> root_object;
  DISALLOW_COPY_AND_ASSIGN(V8NPObject);
};

struct PrivateIdentifier {
  union {
    const NPUTF8* string;
    int32_t number;
  } value;
  bool is_string;
};

NPObject* NPCreateV8ScriptObject(v8::Isolate*, NPP,
    v8::Local<v8::Object>, LocalDOMWindow*);

NPObject* V8ObjectToNPObject(v8::Local<v8::Object>);

bool IsWrappedNPObject(v8::Local<v8::Object>);

CORE_EXPORT V8NPObject* NPObjectToV8NPObject(NPObject*);

void DisposeUnderlyingV8Object(v8::Isolate*, NPObject*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_NPAPI_NP_V8_OBJECT_H_
