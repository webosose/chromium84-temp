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

#include "injection/common/public/renderer/injection_base.h"

#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace injections {

InjectionBase::InjectionBase() {}

InjectionBase::~InjectionBase() {}

void InjectionBase::Dispatch(blink::WebLocalFrame* frame,
                             const std::string& script) {
  if (!frame) {
    return;
  }

  frame->ExecuteScript(blink::WebScriptSource(
      blink::WebString::FromUTF8(script.c_str())));
}

template<>
std::string InjectionBase::JSRepresintation(const std::string& arg) {
  return "'" + arg + "'";
}

template<>
std::string InjectionBase::JSRepresintation(const JSON& arg) {
  return arg.json_;
}

template<>
std::string InjectionBase::JSRepresintation(const bool& arg) {
  return arg ? "true" : "false";
}

template<>
std::string InjectionBase::JSRepresintation(const int& arg) {
  return std::to_string(arg);
}

}  // namespace injections
