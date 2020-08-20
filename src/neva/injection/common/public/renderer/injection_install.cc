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

#include "injection/common/public/renderer/injection_install.h"

#include "base/logging.h"

#if defined(OS_WEBOS)
#include "neva/injection/netcast/netcast_injection.h"
#if defined(USE_GAV)
#include "neva/injection/webosgavplugin/webosgavplugin_injection.h"
#endif  // defined(USE_GAV)
#include "neva/injection/webosservicebridge/webosservicebridge_injection.h"
#include "neva/injection/webossystem/webossystem_injection.h"
#endif  // defined(OS_WEBOS)

#if defined(ENABLE_SAMPLE_WEBAPI)
#include "injection/sample/sample_injection.h"
#endif  // defined(ENABLE_SAMPLE_WEBAPI)

#if defined(ENABLE_BROWSER_CONTROL_WEBAPI)
#include "injection/browser_control/browser_control_injection.h"
#endif  // defined(ENABLE_BROWSER_CONTROL_WEBAPI)

#if defined(ENABLE_MEMORYMANAGER_WEBAPI)
#include "injection/memorymanager/memorymanager_injection.h"
#endif  // defined(ENABLE_MEMORYMANAGER_WEBAPI)

namespace injections {

bool GetInjectionInstallAPI(const std::string& name, InstallAPI* api) {
  DCHECK(api != nullptr);
#if defined(OS_WEBOS)
  if ((name == WebOSSystemWebAPI::kInjectionName) ||
      (name == WebOSSystemWebAPI::kObsoleteName)) {
    api->install_func = WebOSSystemWebAPI::Install;
    api->uninstall_func = WebOSSystemWebAPI::Uninstall;
    return true;
  }

  if ((name == WebOSServiceBridgeWebAPI::kInjectionName) ||
      (name == WebOSServiceBridgeWebAPI::kObsoleteName)) {
    api->install_func = WebOSServiceBridgeWebAPI::Install;
    api->uninstall_func = WebOSServiceBridgeWebAPI::Uninstall;
    return true;
  }

  if (name == NetCastWebAPI::kInjectionName) {
    api->install_func = NetCastWebAPI::Install;
    api->uninstall_func = NetCastWebAPI::Uninstall;
    return true;
  }

#if defined(USE_GAV)
  if (name == WebOSGAVWebAPI::kInjectionName) {
    api->install_func = WebOSGAVWebAPI::Install;
    api->uninstall_func = WebOSGAVWebAPI::Uninstall;
    return true;
  }
#endif  // defined(USE_GAV)
#endif  // defined(OS_WEBOS)

#if defined(ENABLE_SAMPLE_WEBAPI)
  if (name == SampleWebAPI::kInjectionName) {
    api->install_func = SampleWebAPI::Install;
    api->uninstall_func = SampleWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_MEMORYMANAGER_WEBAPI)
  if (name == MemoryManagerWebAPI::kInjectionName) {
    api->install_func = MemoryManagerWebAPI::Install;
    api->uninstall_func = MemoryManagerWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_BROWSER_CONTROL_WEBAPI)
  if (name == BrowserControlWebAPI::kInjectionName) {
    api->install_func = BrowserControlWebAPI::Install;
    api->uninstall_func = BrowserControlWebAPI::Uninstall;
    return true;
  }
#endif
  return false;
}

}  // namespace injections
