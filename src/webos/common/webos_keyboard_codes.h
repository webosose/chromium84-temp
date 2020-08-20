// Copyright 2016-2018 LG Electronics, Inc.
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

#ifndef WEBOS_COMMON_WEBOS_KEYBOARD_CODES_H_
#define WEBOS_COMMON_WEBOS_KEYBOARD_CODES_H_

namespace webos {

enum KeyWebOS {
  Key_webOS_Invalid = 0x00,
  /* 7 Bit Printable ASCII */ \
  Key_0 = 0x30,
  Key_1 = 0x31,
  Key_2 = 0x32,
  Key_3 = 0x33,
  Key_4 = 0x34,
  Key_5 = 0x35,
  Key_6 = 0x36,
  Key_7 = 0x37,
  Key_8 = 0x38,
  Key_9 = 0x39,

  /* Misc Keys */ \
  Key_webOS_Tab       = 0x01000001,
  Key_webOS_Return    = 0x01000004,
  Key_webOS_Left      = 0x01000012,
  Key_webOS_Up        = 0x01000013,
  Key_webOS_Right     = 0x01000014,
  Key_webOS_Down      = 0x01000015,
  Key_webOS_Shift     = 0x01000020,

  /* Media Buttons */ \
  Key_webOS_MediaPlay     = 0x01000080,
  Key_webOS_MediaStop     = 0x01000081,
  Key_webOS_MediaPrevious = 0x01000082,
  Key_webOS_MediaNext     = 0x01000083,
  Key_webOS_MediaRecord   = 0x01000084,
  Key_webOS_MediaPause    = 0x01000085,
  Key_webOS_AudioRewind   = 0x010000c5,
  Key_webOS_AudioForward  = 0x01000102,
  Key_webOS_Subtitle      = 0x01000105,
  /* Volume Button */ \
  Key_webOS_VolumeDown    = 0x01000070,
  Key_webOS_VolumeMute    = 0x01000071,
  Key_webOS_VolumeUp      = 0x01000072,
  /* Misc Buttons */ \
  Key_webOS_Super_L = 0x01000053,
  Key_webOS_Option  = 0x010000e1,
  Key_webOS_Video   = 0x010000f3,
  Key_webOS_Printer = 0x01020002,
  /* webOS Buttons */ \
  Key_webOS_PowerOnOff  = 0x01200000,
  Key_webOS_Exit        = 0x01200001,
  Key_webOS_Info        = 0x01200002,
  Key_webOS_Back        = 0x01200003,
  Key_webOS_Settings    = 0x01200004,
  Key_webOS_Recent      = 0x01200005,
  Key_webOS_Red         = 0x01200011,
  Key_webOS_Green       = 0x01200012,
  Key_webOS_Yellow      = 0x01200013,
  Key_webOS_Blue        = 0x01200014,
  Key_webOS_Twin        = 0x01200015,
  Key_webOS_MagnifierZoom  = 0x01200016,
  Key_webOS_LiveZoom       = 0x01200017,
  Key_webOS_STBMenu        = 0x01200018,
  Key_webOS_STBPower       = 0x01200019,
  Key_webOS_ChannelUp   = 0x01200021,
  Key_webOS_ChannelDown = 0x01200022,
  Key_webOS_ChannelDash = 0x01200023,
  Key_webOS_ChannelBack = 0x01200024,
  Key_webOS_Favorite    = 0x01200025,
  Key_webOS_SetChannel  = 0x01200026,
  Key_webOS_TimerPowerOn = 0x01200027,
  Key_webOS_IVI               = 0x0120002a,
  Key_webOS_3DMode            = 0x01200031,
  Key_webOS_ScreenRemote      = 0x01200032,
  Key_webOS_QMenu             = 0x01200033,
  Key_webOS_Voice             = 0x01200034,
  Key_webOS_InputSource       = 0x01200035,
  Key_webOS_InputTV           = 0x01200036,
  Key_webOS_AspectRatio       = 0x01200037,
  Key_webOS_LiveTVMenu        = 0x01200038,
  Key_webOS_TVGuide           = 0x01200039,
  Key_webOS_AudioDescription  = 0x01200040,
  Key_webOS_MHP               = 0x01200041,
  Key_webOS_Teletext          = 0x01200042,
  Key_webOS_TextOption        = 0x01200043,
  Key_webOS_TextMode          = 0x01200044,
  Key_webOS_TextMix           = 0x01200045,
  Key_webOS_TeletextSubPage   = 0x01200046,
  Key_webOS_TeletextReveal    = 0x01200047,
  Key_webOS_TeletextFreeze    = 0x01200048,
  Key_webOS_TeletextPosition  = 0x01200049,
  Key_webOS_TeletextSize      = 0x01200050,
  Key_webOS_TeletextInTime    = 0x01200051,
  Key_webOS_Simplink          = 0x01200053,
  Key_webOS_MultiPip          = 0x01200054,
  Key_webOS_InputTVRadio      = 0x01200055,
  Key_webOS_ProgramList       = 0x01200056,
  Key_webOS_RecordList        = 0x01200057,
  Key_webOS_StoreMode         = 0x01200058,
  /* Local keys (front panel buttons) */ \
  Key_webOS_LocalUp           = 0x01200061,
  Key_webOS_LocalDown         = 0x01200062,
  Key_webOS_LocalLeft         = 0x01200063,
  Key_webOS_LocalRight        = 0x01200064,
  Key_webOS_LocalEnter        = 0x01200065,
  Key_webOS_LocalLongPress    = 0x01200066,
  Key_webOS_LocalPower        = 0x01200067,
  Key_webOS_LocalVolumeUp     = 0x01200068,
  Key_webOS_LocalVolumeDown   = 0x01200069,
  /* Japan Only */ \
  Key_webOS_BS                = 0x01200091,
  Key_webOS_CS1               = 0x01200092,
  Key_webOS_CS2               = 0x01200093,
  Key_webOS_TER               = 0x01200094,
  Key_webOS_3DigitInput       = 0x01200095,
  Key_webOS_BMLData           = 0x01200096,
  Key_webOS_JapanDisplay      = 0x01200097,
  Key_webOS_BS_1              = 0x01200111,
  Key_webOS_BS_2              = 0x01200112,
  Key_webOS_BS_3              = 0x01200113,
  Key_webOS_BS_4              = 0x01200114,
  Key_webOS_BS_5              = 0x01200115,
  Key_webOS_BS_6              = 0x01200116,
  Key_webOS_BS_7              = 0x01200117,
  Key_webOS_BS_8              = 0x01200118,
  Key_webOS_BS_9              = 0x01200119,
  Key_webOS_BS_10             = 0x0120011a,
  Key_webOS_BS_11             = 0x0120011b,
  Key_webOS_BS_12             = 0x0120011c,
  Key_webOS_CS1_1             = 0x01200121,
  Key_webOS_CS1_2             = 0x01200122,
  Key_webOS_CS1_3             = 0x01200123,
  Key_webOS_CS1_4             = 0x01200124,
  Key_webOS_CS1_5             = 0x01200125,
  Key_webOS_CS1_6             = 0x01200126,
  Key_webOS_CS1_7             = 0x01200127,
  Key_webOS_CS1_8             = 0x01200128,
  Key_webOS_CS1_9             = 0x01200129,
  Key_webOS_CS1_10            = 0x0120012a,
  Key_webOS_CS1_11            = 0x0120012b,
  Key_webOS_CS1_12            = 0x0120012c,
  Key_webOS_CS2_1             = 0x01200131,
  Key_webOS_CS2_2             = 0x01200132,
  Key_webOS_CS2_3             = 0x01200133,
  Key_webOS_CS2_4             = 0x01200134,
  Key_webOS_CS2_5             = 0x01200135,
  Key_webOS_CS2_6             = 0x01200136,
  Key_webOS_CS2_7             = 0x01200137,
  Key_webOS_CS2_8             = 0x01200138,
  Key_webOS_CS2_9             = 0x01200139,
  Key_webOS_CS2_10            = 0x0120013a,
  Key_webOS_CS2_11            = 0x0120013b,
  Key_webOS_CS2_12            = 0x0120013c,
  Key_webOS_TER_1             = 0x01200141,
  Key_webOS_TER_2             = 0x01200142,
  Key_webOS_TER_3             = 0x01200143,
  Key_webOS_TER_4             = 0x01200144,
  Key_webOS_TER_5             = 0x01200145,
  Key_webOS_TER_6             = 0x01200146,
  Key_webOS_TER_7             = 0x01200147,
  Key_webOS_TER_8             = 0x01200148,
  Key_webOS_TER_9             = 0x01200149,
  Key_webOS_TER_10            = 0x0120014a,
  Key_webOS_TER_11            = 0x0120014b,
  Key_webOS_TER_12            = 0x0120014c,
  /* It is not a physical button, but acts like a key */ \
  Key_webOS_CursorShow        = 0x01200201,
  Key_webOS_CursorHide        = 0x01200202,
  Key_webOS_CameraVoice       = 0x01200203,
  /* For commercial model */ \
  Key_webOS_TvLink            = 0x01200301,
  Key_webOS_HotelMode         = 0x01200302,
  Key_webOS_HotelModeReady    = 0x01200303,
  /* Factory keys, It is used to check the specific functionality in the factory,
     or adjust the system internal settings by the developers. */
  Key_webOS_FactoryPowerOnly          = 0x01201001,   /* P-ONLY */
  Key_webOS_FactoryInStart            = 0x01201002,   /* IN START */
  Key_webOS_FactoryInStop             = 0x01201003,   /* IN STOP */
  Key_webOS_FactoryAdjust             = 0x01201004,   /* ADJ */
  Key_webOS_FactoryTv                 = 0x01201010,   /* TV */
  Key_webOS_FactoryVideo1             = 0x01201011,   /* AV1 */
  Key_webOS_FactoryVideo2             = 0x01201012,   /* AV2 */
  Key_webOS_FactoryComponent1         = 0x01201013,   /* COMP1 */
  Key_webOS_FactoryComponent2         = 0x01201014,   /* COMP2 */
  Key_webOS_FactoryHdmi1              = 0x01201015,   /* HDMI1 */
  Key_webOS_FactoryHdmi2              = 0x01201016,   /* HDMI2 */
  Key_webOS_FactoryHdmi3              = 0x01201017,   /* HDMI3 */
  Key_webOS_FactoryHdmi4              = 0x01201018,   /* HDMI4 */
  Key_webOS_FactoryRgbPc              = 0x01201019,   /* RGB */
  Key_webOS_FactoryEyeQ               = 0x01201020,   /* EYE */
  Key_webOS_FactoryPictureMode        = 0x01201021,   /* PSM */
  Key_webOS_FactorySoundMode          = 0x01201022,   /* SSM */
  Key_webOS_FactoryPictureCheck       = 0x01201023,   /* P-CHECK */
  Key_webOS_FactorySoundCheck         = 0x01201024,   /* S-CHECK */
  Key_webOS_FactoryMultiSoundSetting  = 0x01201025,   /* MPX */
  Key_webOS_FactoryTilt               = 0x01201026,   /* TILT */
  Key_webOS_FactoryPip                = 0x01201027,   /* PIP */
  Key_webOS_FactoryHdmiCheck          = 0x01201028,   /* HDMI HOT */
  Key_webOS_FactoryUsbCheck           = 0x01201029,   /* USB HOT */
  Key_webOS_FactoryUsb2Check          = 0x01201030,   /* USB HOT */
  Key_webOS_FactoryPowerOff           = 0x01201031,   /* 'discrete IR power off' */
  Key_webOS_FactoryPowerOn            = 0x01201032,   /* 'discrete IR power on' */
  Key_webOS_FactorySubstrate          = 0x01201033,   /* 'change mode to circuit board product' */
  Key_webOS_FactoryVolume30           = 0x01201034,   /* 'set volume to 30' */
  Key_webOS_FactoryVolume50           = 0x01201035,   /* 'set volume to 50' */
  Key_webOS_FactoryVolume80           = 0x01201036,   /* 'set volume to 80' */
  Key_webOS_FactoryVolume100          = 0x01201037,   /* 'set volume to 100' */
  Key_webOS_FactoryWhiteBalance       = 0x01201038,   /* 'adjust white balance' */
  Key_webOS_Factory3DPattern          = 0x01201039,   /* '3D pattern' */
  Key_webOS_FactorySelfDiagnosis      = 0x01201040,   /* 'self diagnosis' */
  Key_webOS_FactoryPatternCheck       = 0x01201041,   /* 'pattern check on p-only mode' */
  Key_webOS_FactoryQRCheck            = 0x01201042,   /* 'QR code check on p-only mode' */
  /* Another key codes for local keys (power only full-white mode) */
  Key_webOS_FactoryLocalUp            = 0x01201061,
  Key_webOS_FactoryLocalDown          = 0x01201062,
  Key_webOS_FactoryLocalLeft          = 0x01201063,
  Key_webOS_FactoryLocalRight         = 0x01201064,
  Key_webOS_FactoryLocalEnter         = 0x01201065,
  Key_webOS_CecPower                  = 0x01202000,
  Key_webOS_CecMediaHome              = 0x01202001,
  Key_webOS_CecInfoMenu               = 0x01202002,
  Key_webOS_CecInput                  = 0x01202003,
  Key_webOS_CecTitlePopup             = 0x01202004,
  Key_webOS_CecTvGuide                = 0x01202005,
  Key_webOS_CecContentsMenu           = 0x01202006,
  Key_webOS_CecSkipBack10             = 0x01202007,
  Key_webOS_CecSkipForward30          = 0x01202008,
  Key_webOS_MhlScreenRemote           = 0x01202032,
  Key_webOS_VirtualTeleText           = 0x01202042,

  // In goldilocks InputMethodAuraLinux::OnPreeditChanged generates key event for each input from IME
  // This key event can be delivered to app as a keycode 229
  // should handle this keycodes in somewhere to block unexpected keyevent
  // This keycode is not related to LSM
  Key_webOS_IMEProcess = 0x01200401,
};

}  // namespace webos

#endif  // WEBOS_COMMON_WEBOS_KEYBOARD_CODES_H_
