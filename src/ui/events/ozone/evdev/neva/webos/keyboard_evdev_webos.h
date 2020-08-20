// Copyright 2018 LG Electronics, Inc.
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

#ifndef UI_EVENTS_OZONE_EVDEV_NEVA_WEBOS_KEYBOARD_EVDEV_WEBOS_H_
#define UI_EVENTS_OZONE_EVDEV_NEVA_WEBOS_KEYBOARD_EVDEV_WEBOS_H_

#include "ui/events/ozone/evdev/neva/keyboard_evdev_neva.h"

namespace ui {

class KeyboardEvdevWebOS : public KeyboardEvdevNeva {
 public:
  KeyboardEvdevWebOS(EventModifiers* modifiers,
                     KeyboardLayoutEngine* keyboard_layout_engine,
                     const EventDispatchCallback& callback);

  ~KeyboardEvdevWebOS() {}

 private:
  void DispatchKey(unsigned int key,
                   unsigned int scan_code,
                   bool down,
                   bool repeat,
                   base::TimeTicks timestamp,
                   int device_id,
                   int flags) final;

  DISALLOW_COPY_AND_ASSIGN(KeyboardEvdevWebOS);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_NEVA_WEBOS_KEYBOARD_EVDEV_WEBOS_H_
