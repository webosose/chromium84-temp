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

#include "third_party/blink/public/web/web_bindings.h"

#include "third_party/blink/renderer/bindings/core/npapi/np_v8_object.h"
#include "third_party/blink/renderer/bindings/core/npapi/npruntime_impl.h"
#include "third_party/blink/renderer/core/exported/np_plugin.h"

namespace blink {

bool WebBindings::Construct(NPP npp, NPObject* object, const NPVariant* args,
    uint32_t argCount, NPVariant* result) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool ret = _NPN_Construct(npp, object, args, argCount, result);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(ret);
  return ret;
}

NPObject* WebBindings::CreateObject(NPP npp, NPClass* npClass) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  NPObject* result = _NPN_CreateObject(npp, npClass);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(static_cast<void*>(result));
  return result;
}

bool WebBindings::Enumerate(NPP npp, NPObject* object,
    NPIdentifier** identifier, uint32_t* identifierCount) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool result = _NPN_Enumerate(npp, object, identifier, identifierCount);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

bool WebBindings::Evaluate(NPP npp, NPObject* object, NPString* script,
    NPVariant* result) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool ret = _NPN_Evaluate(npp, object, script, result);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(ret);
  return ret;
}

bool WebBindings::EvaluateHelper(NPP npp, bool popupsAllowed, NPObject* object,
    NPString* script, NPVariant* result) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool ret = _NPN_EvaluateHelper(npp, popupsAllowed, object, script, result);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(ret);
  return ret;
}

NPIdentifier WebBindings::GetIntIdentifier(int32_t number) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  NPIdentifier result = _NPN_GetIntIdentifier(number);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(static_cast<void*>(result));
  return result;
}

bool WebBindings::GetProperty(NPP npp, NPObject* object, NPIdentifier property,
   NPVariant* result) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool ret = _NPN_GetProperty(npp, object, property, result);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(ret);
  return ret;
}

NPIdentifier WebBindings::GetStringIdentifier(const NPUTF8* string) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  NPIdentifier result = _NPN_GetStringIdentifier(string);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(static_cast<void*>(result));
  return result;
}

void WebBindings::GetStringIdentifiers(const NPUTF8** names, int32_t nameCount,
    NPIdentifier* identifiers) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  _NPN_GetStringIdentifiers(names, nameCount, identifiers);
  LWP_LOG_MEMBER_FUNCTION_EXIT
}

bool WebBindings::HasMethod(NPP npp, NPObject* object, NPIdentifier method) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool result = _NPN_HasMethod(npp, object, method);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

bool WebBindings::HasProperty(NPP npp, NPObject* object,
  NPIdentifier property) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool result = _NPN_HasProperty(npp, object, property);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

bool WebBindings::IdentifierIsString(NPIdentifier identifier) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool result = _NPN_IdentifierIsString(identifier);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

int32_t WebBindings::IntFromIdentifier(NPIdentifier identifier) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  int32_t result = _NPN_IntFromIdentifier(identifier);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

bool WebBindings::Invoke(NPP npp, NPObject* object, NPIdentifier method,
    const NPVariant* args, uint32_t argCount, NPVariant* result) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool ret = _NPN_Invoke(npp, object, method, args, argCount, result);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(ret);
  return ret;
}

bool WebBindings::InvokeDefault(NPP npp, NPObject* object,
    const NPVariant* args, uint32_t argCount, NPVariant* result) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool ret = _NPN_InvokeDefault(npp, object, args, argCount, result);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(ret);
  return ret;
}

void WebBindings::ReleaseObject(NPObject* object) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  _NPN_ReleaseObject(object);
  LWP_LOG_MEMBER_FUNCTION_EXIT
}

void WebBindings::ReleaseVariantValue(NPVariant* variant) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  _NPN_ReleaseVariantValue(variant);
  LWP_LOG_MEMBER_FUNCTION_EXIT
}

bool WebBindings::RemoveProperty(NPP npp, NPObject* object,
   NPIdentifier identifier) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool result = _NPN_RemoveProperty(npp, object, identifier);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

NPObject* WebBindings::RetainObject(NPObject* object) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  NPObject* result = _NPN_RetainObject(object);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(static_cast<void*>(result));
  return result;
}

void WebBindings::SetException(NPObject* object, const NPUTF8* message) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  _NPN_SetException(object, message);
  LWP_LOG_MEMBER_FUNCTION_EXIT
}

bool WebBindings::SetProperty(NPP npp, NPObject* object,
    NPIdentifier identifier, const NPVariant* value) {
  LWP_LOG_MEMBER_FUNCTION_ENTER;
  bool result = _NPN_SetProperty(npp, object, identifier, value);
  LWP_LOG_MEMBER_FUNCTION_EXIT_RETURN(result);
  return result;
}

NPUTF8* WebBindings::Utf8FromIdentifier(NPIdentifier identifier) {
  NPUTF8* result = _NPN_UTF8FromIdentifier(identifier);
  return result;
}

void WebBindings::ExtractIdentifierData(const NPIdentifier& identifier,
    const NPUTF8*& string, int32_t& number, bool& is_string) {
  PrivateIdentifier* data = static_cast<PrivateIdentifier*>(identifier);
  if (!data) {
    is_string = false;
    number = 0;
    return;
  }

  is_string = data->is_string;
  if (is_string)
    string = data->value.string;
  else
    number = data->value.number;
}

}  // namespace blink
