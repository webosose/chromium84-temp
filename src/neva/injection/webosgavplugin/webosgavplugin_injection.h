// Copyright 2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_INJECTION_WEBOSGAVPLUGIN_WEBOSGAVPLUGIN_INJECTION_H_
#define NEVA_INJECTION_WEBOSGAVPLUGIN_WEBOSGAVPLUGIN_INJECTION_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "neva/injection/common/public/renderer/injection_data_manager.h"
#include "neva/injection/common/public/renderer/injection_install.h"
#include "v8/include/v8.h"

namespace injections {

class COMPONENT_EXPORT(INJECTION) WebOSGAVWebAPI {
 public:
  static const char kInjectionName[];
  static const char kInjectionObjectName[];
  static void Install(blink::WebLocalFrame* frame);
  static void Uninstall(blink::WebLocalFrame* frame);

 private:
  static v8::MaybeLocal<v8::Object> CreateWebOSGAVObject(
      blink::WebLocalFrame* frame,
      v8::Isolate* isolate,
      v8::Local<v8::Object> global);
};

}  // namespace injections

#endif  // NEVA_INJECTION_WEBOSGAVPLUGIN_WEBOSGAVPLUGIN_INJECTION_H_
