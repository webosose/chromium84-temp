// Copyright 2016-2019 LG Electronics, Inc.
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

#ifndef NEVA_INJECTION_COMMON_PUBLIC_RENDERER_INJECTION_BASE_H_
#define NEVA_INJECTION_COMMON_PUBLIC_RENDERER_INJECTION_BASE_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace injections {

class CONTENT_EXPORT InjectionBase {
 public:
  InjectionBase();
  virtual ~InjectionBase();

  static void Dispatch(blink::WebLocalFrame* frame,
                       const std::string& script);

  // Call JavaScript function [function] with arguments [arguments].
  // Note:
  // integer arguments will be converted to string representation,
  // bool will be converted to true or false,
  // std::string will be taken into quotes.
  // To pass json object (string "{type:value}") JSON wrapper should be used.
  // So call CallJSFunction(frame, "someFunction", JSON(value)) will pass value
  // into JavaScript as is.
  template <typename... Types>
  static void CallJSFunction(blink::WebLocalFrame* frame,
                             const std::string& function,
                             Types... arguments) {
    std::string call_script = function;
    call_script += "(";
    call_script += CombineArguments(arguments...);
    call_script += ");";
    InjectionBase::Dispatch(frame, call_script);
  }

  // Call JavaScript function [function] without arguments
  static void CallJSFunction(blink::WebLocalFrame* frame,
                             const std::string& function) {
    InjectionBase::Dispatch(frame, function + "()");
  }

 protected:
  struct JSON {
    JSON(const std::string& json): json_(json) {}

    const std::string& json_;
  };

 private:
  template <typename Type>
  static std::string JSRepresintation(const Type& arg);

  template <typename Type>
  static std::string CombineArguments(const Type& first) {
    return JSRepresintation(first);
  }

  template <typename Type, typename... Rest>
  static std::string CombineArguments(const Type& first, Rest... rest) {
    return JSRepresintation(first) + ", " + CombineArguments(rest...);
  }
};

}  // namespace injections

#endif  // NEVA_INJECTION_COMMON_PUBLIC_RENDERER_INJECTION_BASE_H_
