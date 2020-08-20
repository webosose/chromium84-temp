// Copyright 2020 LG Electronics, Inc.
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

#include "neva/pal_service/webos/platform_system_delegate_webos.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace pal {

namespace webos {

PlatformSystemDelegateWebOS::PlatformSystemDelegateWebOS() = default;

PlatformSystemDelegateWebOS::~PlatformSystemDelegateWebOS() = default;

std::string PlatformSystemDelegateWebOS::GetCountry() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string PlatformSystemDelegateWebOS::GetDeviceInfoJSON() const {
  NOTIMPLEMENTED();
  return std::string();
}

double PlatformSystemDelegateWebOS::DevicePixelRatio() const {
  NOTIMPLEMENTED();
  return -1.;
}

std::string PlatformSystemDelegateWebOS::GetLocale() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string PlatformSystemDelegateWebOS::GetLocaleRegion() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string PlatformSystemDelegateWebOS::GetResource(
    const std::string& path) const {
  NOTIMPLEMENTED();
  return std::string();
}

bool PlatformSystemDelegateWebOS::IsMinimal() const {
  return base::PathExists(
      base::FilePath("/var/luna/preferences/ran-firstuse"));
}

}  // namespace webos

}  // namespace pal
