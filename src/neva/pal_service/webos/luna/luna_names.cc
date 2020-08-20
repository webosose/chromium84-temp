// Copyright 2019 LG Electronics, Inc.
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

#include "neva/pal_service/webos/luna/luna_names.h"

#include "base/logging.h"
#include "base/rand_util.h"

#include <stdlib.h>
#include <unistd.h>

namespace pal {
namespace luna {

namespace {

const char kLunaScheme[] = "luna://";
const char KPalmScheme[] = "palm://";

}  // namespace

namespace service_uri {

const char kVSM[] = "com.webos.service.videosinkmanager";
const char kDisplay[] = "com.webos.service.tv.display";
const char kAVBlock[] = "com.webos.service.tv.avblock";
const char kAudio[] = "com.webos.audio";
const char kBroadcast[] = "com.webos.service.utp.broadcast";
const char kChannel[] = "com.webos.service.utp.channel";
const char kExternalDevice[] =  "com.webos.service.tv.externaldevice";
const char kDVR[] = "com.webos.service.tv.dvr";
const char kSound[] = "com.webos.service.tv.sound";
const char kSubtitle[] = "com.webos.service.tv.subtitle";
const char kDRM[] = "com.webos.service.drm";
const char kSetting[] = "com.webos.settingsservice";
const char kPhotoRenderer[] = "com.webos.service.photorenderer";
const char kMemoryManager[] = "com.webos.memorymanager";
const char kPalmBus[] = "com.palm.bus";
const char kApplicationManager[] = "com.webos.applicationManager";

}  // namespace service_uri

namespace service_name {

const char kChromiumMedia[] = "com.webos.chromium.media";
const char kChromiumMemory[] = "com.webos.chromium.memory";

}  // namespace service_name

std::string GetServiceURI(const char* uri, const char* action) {
  std::string result(kLunaScheme);
  return result.append(uri).append("/").append(action);
}

std::string GetServiceNameWithRandSuffix(const char* name) {
  std::string result(name);
  return result.append(".").append(std::to_string(base::RandInt(10000, 99999)));
}

std::string GetServiceNameWithPID(const char* name) {
  std::string result(name);
  return result.append("-").append(std::to_string(getpid()));
}

}  // namespace luna
}  // namespace pal
