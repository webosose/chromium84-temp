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

#ifndef UI_EVENTS_KEYCODES_WEBOS_KEYCODE_CONVERTER_H_
#define UI_EVENTS_KEYCODES_WEBOS_KEYCODE_CONVERTER_H_

#include <stddef.h>
#include <stdint.h>

namespace ui {

typedef struct {
  uint32_t hw_keycode;
  uint32_t lge_keycode;
  uint32_t posix_keycode;
} LGHWKeycodeMapEntry;

typedef struct {
  uint32_t lge_keycode;
  uint32_t webos_keycode;
  uint32_t posix_keycode;
} LGWebOSKeycodeMapEntry;

class KeycodeConverterWebOS {
 public:
  static uint32_t LGKeyCodeFromEvdev(uint32_t keycode);
  static uint32_t WebOSKeyCodeFromLG(uint32_t keycode);
  static bool IsKeyCodeNonPrintable(uint16_t keycode);
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_WEBOS_KEYCODE_CONVERTER_H_
