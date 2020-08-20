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

#include "ui/events/keycodes/webos/keycode_converter.h"

#include "base/macros.h"
#include "base/stl_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/keycodes/webos/lgtv_keyboard_codes.h"

namespace ui {

uint32_t KeycodeConverterWebOS::LGKeyCodeFromEvdev(uint32_t keycode) {
#define LG_KEYMAP(hardware, LGKeyCode, webos, posix, code) \
  { hardware, LGKeyCode, posix }
#define LG_KEYMAP_DECLARATION const LGHWKeycodeMapEntry lg_hw_keycode_map[] =
#include "ui/events/keycodes/webos/keycode_converter_data.inc"
#undef LG_KEYMAP
#undef LG_KEYMAP_DECLARATION
  const size_t kLGHWKeycodeMapEntries = base::size(lg_hw_keycode_map);

  for (size_t i = 0; i < kLGHWKeycodeMapEntries; i++) {
    if (lg_hw_keycode_map[i].hw_keycode == keycode) {
      uint32_t native_keycode = 0;
      if (lg_hw_keycode_map[i].lge_keycode)
        native_keycode = lg_hw_keycode_map[i].lge_keycode;
      else if (lg_hw_keycode_map[i].posix_keycode)
        native_keycode = lg_hw_keycode_map[i].posix_keycode;
      return native_keycode;
    }
  }
  return 0;
}

uint32_t KeycodeConverterWebOS::WebOSKeyCodeFromLG(uint32_t keycode) {
  // This converting from dom keycode to webOS keycode is confusing
  // ex) dom keycode 'END'(0x23) is for real END key on HID keyboard
  // but in JP model, key '12' is converted to 0x23(END)
  // But this function is used in WAM/pluggable side
  // - to filter key event
  // - to give exceptional key event converting
  // - to allow forbidden key event to CP
  // So webOS TV specific conversion is prior
  // If WAM checks other key then has to add key code here

#define LG_KEYMAP(hardware, LGKeyCode, webos, posix, code) \
  { LGKeyCode, webos, posix }
#define LG_KEYMAP_DECLARATION \
  const LGWebOSKeycodeMapEntry lg_webos_keycode_map[] =
#include "ui/events/keycodes/webos/keycode_converter_data.inc"
#undef LG_KEYMAP
#undef LG_KEYMAP_DECLARATION

  const size_t kLGWebOSKeycodeMapEntries = base::size(lg_webos_keycode_map);
  for (size_t i = 0; i < kLGWebOSKeycodeMapEntries; i++) {
    if (lg_webos_keycode_map[i].lge_keycode == keycode)
      return lg_webos_keycode_map[i].webos_keycode;
    else if (lg_webos_keycode_map[i].posix_keycode == keycode)
      return lg_webos_keycode_map[i].webos_keycode;
  }

  return keycode;
}

bool KeycodeConverterWebOS::IsKeyCodeNonPrintable(uint16_t keycode) {
  switch (keycode) {
    case VKEY_PLAY:
    case VK_LGE_RCU_PLAY:
    case VKEY_PAUSE:
    case VK_LGE_RCU_STOP:
    case VK_LGE_RCU_REWIND:
    case VK_LGE_RCU_FASTFORWARD:
    case VK_LGE_RCU_RECORD:
    case VK_LGE_RCU_TRACK_PREV:
    case VK_LGE_RCU_TRACK_NEXT:
    case VK_LGE_RCU_SUBTITLE:
    case VK_LGE_RCU_BACK:
    case VKEY_BROWSER_BACK:
    case VK_LGE_RCU_INFO:
    case VK_LGE_RCU_EXIT:
    case VK_LGE_RCU_SETTINGS:
    case VK_LGE_RCU_HOME:
    case VK_LGE_RCU_RED:
    case VK_LGE_RCU_GREEN:
    case VK_LGE_RCU_YELLOW:
    case VK_LGE_RCU_BLUE:
    case VKEY_PRIOR:
    case VKEY_NEXT:
    case VK_LGE_RCU_DASH:
    case VK_LGE_RCU_FLASHBACK:
    case VK_LGE_RCU_FAVORITES:
    case VK_LGE_RCU_3D:
    case VK_LGE_RCU_QMENU:
    case VK_LGE_RCU_TV_VIDEO:
    case VK_LGE_RCU_TV_RADIO:
    case VK_LGE_RCU_RATIO:
    case VK_LGE_RCU_LIVETV:
    case VK_LGE_RCU_GUIDE:
    case VK_LGE_RCU_MHP:
    case VK_LGE_RCU_OPTION:
    case VK_LGE_RCU_3DIGIT_INPUT:
    case VK_LGE_RCU_SETCHANNEL:
    case VK_LGE_RCU_BML_DATA:
    case VK_LGE_RCU_JAPAN_DISPLAY:
    case VK_LGE_RCU_TELETEXT:
    case VK_LGE_RCU_TEXTMODE:
    case VK_LGE_RCU_TEXTMIX:
    case VK_LGE_RCU_TELETEXT_SUBPAGE:
    case VK_LGE_RCU_TELETEXT_REVEAL:
    case VK_LGE_RCU_TELETEXT_FREEZE:
    case VK_LGE_RCU_TELETEXT_POSITION:
    case VK_LGE_RCU_TELETEXT_SIZE:
    case VK_LGE_RCU_TELETEXT_INTIME:
    case VK_LGE_RCU_AUDIODESC:
    case VK_LGE_RCU_SIMPLINK:
    case VK_LGE_RCU_MULTI_PIP:
    case VK_LGE_RCU_PRLIST:
    case VK_LGE_RCU_SCREENREMOTE:
    case VK_LGE_RCU_LIVE_ZOOM:
    case VK_LGE_RCU_MAGNIFIER_ZOOM:
    // TV special keycodes
    case VK_LGE_CURSOR_SHOW:
    case VK_LGE_CURSOR_HIDE:
    case VK_LGE_RCU_IVI:
    // Factory remocon keycodes
    case VK_LGE_FACTORY_EYEQ:
    case VK_LGE_FACTORY_POWERON:
    // Japan remocon keycodes
    case VK_LGE_RCU_BS:
    case VK_LGE_RCU_CS1:
    case VK_LGE_RCU_CS2:
    case VK_LGE_RCU_TER:
    case VKEY_PRINT:
    case VKEY_PROCESSKEY:
      return true;

    // Key code is not system
    default:
      return false;
  }
}

}  // namespace ui
