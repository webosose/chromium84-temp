/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_BINDINGS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_BINDINGS_H_

#include "third_party/blink/public/platform/web_common.h"
#include "neva/npapi/bindings/npruntime.h"

namespace blink {

// A haphazard collection of functions for dealing with plugins.
class WebBindings {
 public:
  // NPN Functions ------------------------------------------------------
  // These are all defined in npruntime.h and are well documented.

  // NPN_Construct
  BLINK_EXPORT static bool Construct(NPP, NPObject*, const NPVariant* args,
      uint32_t argCount, NPVariant* result);

  // NPN_CreateObject
  BLINK_EXPORT static NPObject* CreateObject(NPP, NPClass*);

  // NPN_Enumerate
  BLINK_EXPORT static bool Enumerate(NPP, NPObject*, NPIdentifier**,
      uint32_t* identifierCount);

  // NPN_Evaluate
  BLINK_EXPORT static bool Evaluate(NPP, NPObject*, NPString* script,
      NPVariant* result);

  // NPN_EvaluateHelper
  BLINK_EXPORT static bool EvaluateHelper(NPP, bool popupsAllowed, NPObject*,
      NPString* script, NPVariant* result);

  // NPN_GetIntIdentifier
  BLINK_EXPORT static NPIdentifier GetIntIdentifier(int32_t number);

  // NPN_GetProperty
  BLINK_EXPORT static bool GetProperty(NPP, NPObject*,
      NPIdentifier property, NPVariant *result);

  // NPN_GetStringIdentifier
  BLINK_EXPORT static NPIdentifier GetStringIdentifier(const NPUTF8* string);

  // NPN_GetStringIdentifiers
  BLINK_EXPORT static void GetStringIdentifiers(const NPUTF8** names,
      int32_t nameCount, NPIdentifier*);

  // NPN_HasMethod
  BLINK_EXPORT static bool HasMethod(NPP, NPObject*, NPIdentifier method);

  // NPN_HasProperty
  BLINK_EXPORT static bool HasProperty(NPP, NPObject*, NPIdentifier property);

  // NPN_IdentifierIsString
  BLINK_EXPORT static bool IdentifierIsString(NPIdentifier);

  // NPN_IntFromIdentifier
  BLINK_EXPORT static int32_t IntFromIdentifier(NPIdentifier);

  // NPN_Invoke
  BLINK_EXPORT static bool Invoke(NPP, NPObject*, NPIdentifier method,
      const NPVariant* args, uint32_t argCount, NPVariant* result);

  // NPN_InvokeDefault
  BLINK_EXPORT static bool InvokeDefault(NPP, NPObject*, const NPVariant* args,
      uint32_t argCount, NPVariant* result);

  // NPN_ReleaseObject
  BLINK_EXPORT static void ReleaseObject(NPObject*);

  // NPN_ReleaseVariantValue
  BLINK_EXPORT static void ReleaseVariantValue(NPVariant*);

  // NPN_RemoveProperty
  BLINK_EXPORT static bool RemoveProperty(NPP, NPObject*, NPIdentifier);

  // NPN_RetainObject
  BLINK_EXPORT static NPObject* RetainObject(NPObject*);

  // NPN_SetException
  BLINK_EXPORT static void SetException(NPObject*, const NPUTF8* message);

  // NPN_SetProperty
  BLINK_EXPORT static bool SetProperty(NPP, NPObject*, NPIdentifier,
      const NPVariant*);

  // NPN_UTF8FromIdentifier
  BLINK_EXPORT static NPUTF8* Utf8FromIdentifier(NPIdentifier);

  // Miscellaneous utility functions ----------------------------------------

  // Complement to NPN_Get___Identifier functions.
  // Extracts data from the NPIdentifier data
  // structure.  If isString is true upon return,
  // string will be set but number's value is
  // undefined.  If isString is false, the opposite is true.
  BLINK_EXPORT static void ExtractIdentifierData(const NPIdentifier&,
      const NPUTF8*& string, int32_t& number, bool& is_string);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_BINDINGS_H_
