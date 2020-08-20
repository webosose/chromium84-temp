// Copyright 2018-2019 LG Electronics, Inc.
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

#include "ui/events/ozone/evdev/neva/webos/keyboard_evdev_webos.h"

#include "ui/events/event.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/webos/keycode_converter.h"
#include "ui/events/keycodes/webos/lgtv_keyboard_codes.h"

namespace ui {

KeyboardEvdevWebOS::KeyboardEvdevWebOS(
    EventModifiers* modifiers,
    KeyboardLayoutEngine* keyboard_layout_engine,
    const EventDispatchCallback& callback)
    : KeyboardEvdevNeva(modifiers, keyboard_layout_engine, callback) {
  SetAutoRepeatEnabled(false);
}

void KeyboardEvdevWebOS::DispatchKey(unsigned int key,
                                     unsigned int scan_code,
                                     bool down,
                                     bool repeat,
                                     base::TimeTicks timestamp,
                                     int device_id,
                                     int flags) {
  uint32_t lg_code = KeycodeConverterWebOS::LGKeyCodeFromEvdev(key);
  if (lg_code) {
    KeyEvent event(down ? ET_KEY_PRESSED : ET_KEY_RELEASED,
            static_cast<KeyboardCode>(lg_code), DomCode::NONE,
            modifiers_->GetModifierFlags(), DomKey(), timestamp);
    event.set_source_device_id(device_id);
    callback_.Run(&event);
    return;
  }

  KeyboardEvdev::DispatchKey(key, scan_code, down, repeat, timestamp, device_id,
                             flags);
}

}  // namespace ui
