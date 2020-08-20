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

#ifndef NEVA_INJECTION_WEBOSSYSTEM_WEBOSSYSTEM_INJECTION_H_
#define NEVA_INJECTION_WEBOSSYSTEM_WEBOSSYSTEM_INJECTION_H_

#include "base/component_export.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
}

namespace injections {

class COMPONENT_EXPORT(INJECTION) WebOSSystemWebAPI {
 public:
  static const char kInjectionName[];
  static const char kObsoleteName[];

  static void Install(blink::WebLocalFrame* frame);
  static void Uninstall(blink::WebLocalFrame* frame);

 private:
  static v8::MaybeLocal<v8::Object> CreateWebOSSystemObject(
      blink::WebLocalFrame* frame,
      v8::Isolate* isolate,
      v8::Local<v8::Object> global);
};

}  // namespace injections

#endif  // NEVA_INJECTION_WEBOSSYSTEM_WEBOSSYSTEM_INJECTION_H_
