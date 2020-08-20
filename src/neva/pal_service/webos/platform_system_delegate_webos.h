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

#ifndef NEVA_PAL_SERVICE_WEBOS_PLATFORM_SYSTEM_DELEGATE_WEBOS_H_
#define NEVA_PAL_SERVICE_WEBOS_PLATFORM_SYSTEM_DELEGATE_WEBOS_H_

#include <string>

#include "neva/pal_service/platform_system_delegate.h"

namespace pal {

namespace webos {

class PlatformSystemDelegateWebOS : public PlatformSystemDelegate {
 public:
  PlatformSystemDelegateWebOS();
  ~PlatformSystemDelegateWebOS() override;

  PlatformSystemDelegateWebOS(const PlatformSystemDelegateWebOS&) = delete;
  PlatformSystemDelegateWebOS& operator = (const PlatformSystemDelegateWebOS&) =
      delete;

  std::string GetCountry() const override;
  std::string GetDeviceInfoJSON() const override;
  double DevicePixelRatio() const override;
  std::string GetLocale() const override;
  std::string GetLocaleRegion() const override;
  std::string GetResource(const std::string& path) const override;
  bool IsMinimal() const override;
};

}  // namespace webos

}  // namespace pal

#endif  // NEVA_PAL_SERVICE_PLATFORM_SYSTEM_DELEGATE_WEBOS_H_
