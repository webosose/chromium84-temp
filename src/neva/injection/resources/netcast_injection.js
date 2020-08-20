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

NetCast.netCastLaunchQMENU = function() {
  return NetCast.LaunchSystemUI("QMENU");
};

NetCast.netCastLaunch3DMENU = function() {
  return NetCast.LaunchSystemUI("3DMENU");
};

NetCast.netCastLaunchRATIO = function() {
  return NetCast.LaunchSystemUI("RATIO");
};

NetCast._netCastPromotionStatus = function(arg) {
  if(NetCast._isPromotionStatusCallback.length > 0) {
    NetCast._isPromotionStatusCallback.forEach(function(callback) {
      if(typeof callback == 'function')
        callback(arg);
      else if(typeof callback == 'string')
        eval(callback + '(' + arg + ');');
      else
        console.log(
            '[netcast_injection] callback is NOT function or string; return;');
    });
  }
  else
    console.log('[netcast_injection] There is NOT any callback registered');
};

NetCastReturn = function(arg) {
  var BACK = 461;
  var EXIT = 1000;
  if(arg == BACK)
    window.NetCastBack();
  else if(arg == EXIT)
    window.NetCastExit();
  else
    console.log('[ERROR] NetCastReturn() wrong parameter : ' + arg);
}

NetCastExit = function() {
  return window.close();
}

NetCastBack = function() {
  return NetCast.netCastBack();
}

NetCastSetDefaultAspectRatio = function(arg) {
  return NetCast.netCastSetDefaultAspectRatio(arg);
}

NetCastLaunchQMENU = function() {
  return NetCast.netCastLaunchQMENU();
}

NetCastLaunch3DMENU = function() {
  return NetCast.netCastLaunch3DMENU();
}

NetCastLaunchRATIO = function() {
  return NetCast.netCastLaunchRATIO();
}

NetCastGetMouseOnOff = function() {
  return NetCast.netCastGetMouseOnOff();
}

NetCastGetURLForCP = function(arg) {
  return NetCast.netCastGetURLForCP(arg);
}

NetCastSystemKeyboardVisible = function(arg) {
  return NetCast.netCastSystemKeyboardVisible(arg);
}

NetCastOpenWebBrowser = function(arg) {
  return NetCast.netCastOpenWebBrowser(arg);
}

NetCastKeepAlive = function(arg) {
  return NetCast.netCastKeepAlive(arg);
}

NetCastGetPreferences = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCall("palm://com.palm.systemservice/getPreferences", arg1, arg2, arg3);
}

NetCastTelluriumSubscribeToCommands = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCall("palm://com.palm.service.tellurium/subscribeToCommands", arg1, arg2, arg3);
}

NetCastTelluriumNotifyEvent = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCall("palm://com.palm.service.tellurium/notifyEvent", arg1, arg2, arg3);
}

NetCastTelluriumReplyToCommand = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCall("palm://com.palm.service.tellurium/replyToCommand", arg1, arg2, arg3);
}

NetCastEnableSubtitle = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCall("luna://com.webos.service.tv.subtitle/enableSubtitle", arg1, arg2, arg3);
}

NetCastCreateAppChannel = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCall("palm://com.webos.service.secondscreen.gateway/app2app/createAppChannel", arg1, arg2, arg3);
}

NetCastCancelGetPreferences = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCancel("palm://com.palm.systemservice/getPreferences");
}

NetCastCancelTelluriumSubscribeToCommands = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCancel("palm://com.palm.service.tellurium/subscribeToCommands");
}

NetCastCancelTelluriumNotifyEvent = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCancel("palm://com.palm.service.tellurium/notifyEvent");
}

NetCastCancelTelluriumReplyToCommand = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCancel("palm://com.palm.service.tellurium/replyToCommand");
}

NetCastCancelEnableSubtitle = function(arg1, arg2, arg3) {
  return NetCast.webOSServiceCancel("luna://com.webos.service.tv.subtitle/enableSubtitle");
}

NetCastSetCursorImage = function(arg1, arg2, arg3) {
  if (!arg2)
    arg2 = -1;
  if (!arg3)
    arg3 = -1;
  return NetCast.netCastSetCursorImage(arg1, arg2, arg3);
}

NetCastIsPromotionStatus = function(arg) {
  var string_callback;
  if (typeof(arg) == 'function' || typeof(arg) == 'string') {
    NetCast._isPromotionStatusCallback.push(arg);
    if(NetCast._isPromotionStatusCallback.length > 1)
      return;
    string_callback = 'NetCast._netCastPromotionStatus';
    return NetCast.netCastIsPromotionStatus(string_callback);
  }
  return false;
}

getIdentifier = function() {
  return NetCast.netCastIdentifier();
}

if (typeof(identifier) == 'undefined') {
  Object.defineProperty(this, "identifier", {
    get: function() {
      return NetCast.netCastIdentifier();
    }
  });
}

if (typeof(launchParams) == 'undefined') {
  Object.defineProperty(this, "launchParams", {
    get: function() {
      return NetCast.netCastLaunchParams();
    }
  });
}

window.originalDevicePixelRatio =
  Object.getOwnPropertyDescriptor(window, "devicePixelRatio");

Object.defineProperty(window, "devicePixelRatio", {
  get: function() {
    return NetCast.devicePixelRatio();
  }
});

if (typeof(KeyEvent) == 'undefined') {
  KeyEvent = {
    "VK_UNDEFINED"               : 0,
    "VK_CANCEL"                  : 3,
    "VK_BACK_SPACE"              : 8,
    "VK_TAB"                     : 9,
    "VK_CLEAR"                   : 12,
    "VK_ENTER"                   : 13,
    "VK_SHIFT"                   : 16,
    "VK_CONTROL"                 : 17,
    "VK_ALT"                     : 18,
    "VK_PAUSE"                   : 19,
    "VK_CAPS_LOCK"               : 20,
    "VK_KANA"                    : 21,
    "VK_FINAL"                   : 24,
    "VK_KANJI"                   : 25,
    "VK_ESCAPE"                  : 27,
    "VK_CONVERT"                 : 28,
    "VK_NONCONVERT"              : 29,
    "VK_ACCEPT"                  : 30,
    "VK_MODECHANGE"              : 31,
    "VK_SPACE"                   : 32,
    "VK_PAGE_UP"                 : 33,
    "VK_PAGE_DOWN"               : 34,
    "VK_END"                     : 35,
    "VK_HOME"                    : 36,
    "VK_LEFT"                    : 37,
    "VK_UP"                      : 38,
    "VK_RIGHT"                   : 39,
    "VK_DOWN"                    : 40,
    "VK_COMMA"                   : 44,
    "VK_PERIOD"                  : 46,
    "VK_SLASH"                   : 47,
    "VK_0"                       : 48,
    "VK_1"                       : 49,
    "VK_2"                       : 50,
    "VK_3"                       : 51,
    "VK_4"                       : 52,
    "VK_5"                       : 53,
    "VK_6"                       : 54,
    "VK_7"                       : 55,
    "VK_8"                       : 56,
    "VK_9"                       : 57,
    "VK_SEMICOLON"               : 59,
    "VK_EQUALS"                  : 61,
    "VK_A"                       : 65,
    "VK_B"                       : 66,
    "VK_C"                       : 67,
    "VK_D"                       : 68,
    "VK_E"                       : 69,
    "VK_F"                       : 70,
    "VK_G"                       : 71,
    "VK_H"                       : 72,
    "VK_I"                       : 73,
    "VK_J"                       : 74,
    "VK_K"                       : 75,
    "VK_L"                       : 76,
    "VK_M"                       : 77,
    "VK_N"                       : 78,
    "VK_O"                       : 79,
    "VK_P"                       : 80,
    "VK_Q"                       : 81,
    "VK_R"                       : 82,
    "VK_S"                       : 83,
    "VK_T"                       : 84,
    "VK_U"                       : 85,
    "VK_V"                       : 86,
    "VK_W"                       : 87,
    "VK_X"                       : 88,
    "VK_Y"                       : 89,
    "VK_Z"                       : 90,
    "VK_OPEN_BRACKET"            : 91,
    "VK_BACK_SLASH"              : 92,
    "VK_CLOSE_BRACKET"           : 93,
    "VK_NUMPAD0"                 : 96,
    "VK_NUMPAD1"                 : 97,
    "VK_NUMPAD2"                 : 98,
    "VK_NUMPAD3"                 : 99,
    "VK_NUMPAD4"                 : 100,
    "VK_NUMPAD5"                 : 101,
    "VK_NUMPAD6"                 : 102,
    "VK_NUMPAD7"                 : 103,
    "VK_NUMPAD8"                 : 104,
    "VK_NUMPAD9"                 : 105,
    "VK_MULTIPLY"                : 106,
    "VK_ADD"                     : 107,
    "VK_SEPARATER"               : 108,
    "VK_SUBTRACT"                : 109,
    "VK_DECIMAL"                 : 110,
    "VK_DIVIDE"                  : 111,
    "VK_F1"                      : 112,
    "VK_F2"                      : 113,
    "VK_F3"                      : 114,
    "VK_F4"                      : 115,
    "VK_F5"                      : 116,
    "VK_F6"                      : 117,
    "VK_F7"                      : 118,
    "VK_F8"                      : 119,
    "VK_F9"                      : 120,
    "VK_F10"                     : 121,
    "VK_F11"                     : 122,
    "VK_F12"                     : 123,
    "VK_DELETE"                  : 127,
    "VK_NUM_LOCK"                : 144,
    "VK_SCROLL_LOCK"             : 145,
    "VK_PRINTSCREEN"             : 154,
    "VK_INSERT"                  : 155,
    "VK_HELP"                    : 156,
    "VK_META"                    : 157,
    "VK_BACK_QUOTE"              : 192,
    "VK_QUOTE"                   : 222,
    "VK_RED"                     : 403,
    "VK_GREEN"                   : 404,
    "VK_YELLOW"                  : 405,
    "VK_BLUE"                    : 406,
    "VK_GREY"                    : 407,
    "VK_BROWN"                   : 408,
    "VK_POWER"                   : 409,
    "VK_DIMMER"                  : 410,
    "VK_WINK"                    : 411,
    "VK_REWIND"                  : 412,
    "VK_STOP"                    : 413,
    "VK_EJECT_TOGGLE"            : 414,
    "VK_PLAY"                    : 415,
    "VK_RECORD"                  : 416,
    "VK_FAST_FWD"                : 417,
    "VK_PLAY_SPEED_UP"           : 418,
    "VK_PLAY_SPEED_DOWN"         : 419,
    "VK_PLAY_SPEED_RESET"        : 420,
    "VK_RECORD_SPEED_NEXT"       : 421,
    "VK_GO_TO_START"             : 422,
    "VK_GO_TO_END"               : 423,
    "VK_TRACK_PREV"              : 424,
    "VK_TRACK_NEXT"              : 425,
    "VK_RANDOM_TOGGLE"           : 426,
    "VK_CHANNEL_UP"              : 427,
    "VK_CHANNEL_DOWN "           : 428,
    "VK_STORE_FAVORITE_0"        : 429,
    "VK_STORE_FAVORITE_1"        : 430,
    "VK_STORE_FAVORITE_2"        : 431,
    "VK_STORE_FAVORITE_3"        : 432,
    "VK_RECALL_FAVORITE_0"       : 433,
    "VK_RECALL_FAVORITE_1"       : 434,
    "VK_RECALL_FAVORITE_2"       : 435,
    "VK_RECALL_FAVORITE_3"       : 436,
    "VK_CLEAR_FAVORITE_0"        : 437,
    "VK_CLEAR_FAVORITE_1"        : 438,
    "VK_CLEAR_FAVORITE_2"        : 439,
    "VK_CLEAR_FAVORITE_3"        : 440,
    "VK_SCAN_CHANNELS_TOGGLE"    : 441,
    "VK_PINP_TOGGLE"             : 442,
    "VK_SPLIT_SCREEN_TOGGLE"     : 443,
    "VK_DISPLAY_SWAP"            : 444,
    "VK_SCREEN_MODE_NEXT"        : 445,
    "VK_VIDEO_MODE_NEXT"         : 446,
    "VK_VOLUME_UP"               : 447,
    "VK_VOLUME_DOWN"             : 448,
    "VK_MUTE"                    : 449,
    "VK_SURROUND_MODE_NEXT"      : 450,
    "VK_BALANCE_RIGHT"           : 451,
    "VK_BALANCE_LEFT"            : 452,
    "VK_FADER_FRONT"             : 453,
    "VK_FADER_REAR"              : 454,
    "VK_BASS_BOOST_UP"           : 455,
    "VK_BASS_BOOST_DOWN"         : 456,
    "VK_INFO"                    : 457,
    "VK_GUIDE"                   : 458,
    "VK_TELETEXT"                : 459,
    "VK_SUBTITLE"                : 460,
    "VK_BACK"                    : 461,
    "VK_MENU"                    : 462
   }
}

// Some apps(US, LifeShow), they call this function, to avoid exception
NetCastSetScreenSaver = function() {
  return true;
}

// All deprecated
NetCastSetPageLoadingIcon = function(arg) {
  return false;
}

NetCastMouseOff = function(arg) {
  return false;
}

NetCastActivateSmartText = function(arg1, arg2) {
  return false;
}

NetCastEnableMagicRCU = function() {
  return false;
}

NetCastDisableMagicRCU = function() {
  return false;
}

NetCastGetMagicRCUAvailability = function() {
  return false;
}

NetCastTVSet = function(arg1, arg2, arg3, arg4) {
  return false;
}

NetCastEnableNumberKey = function(arg1) {
  return false;
}

NetCastEnableChannelKey = function(arg1) {
  return false;
}

NetCastGetUsedMemorySize = function() {
  return 0;
}

NetCastSet3DMode = function(arg) {
  return false;
}

NetCastGet3DMode = function() {
  return false;
}

NetCastTriDKeyEnable = function(arg) {
  return false;
}

NetCastIsSupport3D = function() {
  return false;
}

NetCastEnable3DMode = function(arg) {
  return false;
}

NetCastHIDInitialize = function(arg) {
  return false;
}

NetCastHIDLanguageList = function(arg) {
  return false;
}

NetCastHIDChangeLanguage = function(arg) {
  return false;
}

NetCastSystemKeyboardSession = function(arg) {
  return false;
}
