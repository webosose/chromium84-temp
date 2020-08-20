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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_INFO_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace blink {

struct LegacyWebPluginMimeType {
  // The name of the mime type (e.g., "application/x-shockwave-flash").
  std::string mime_type;

  // A list of all the file extensions for this mime type.
  std::vector<std::string> file_extensions;

  // Description of the mime type.
  base::string16 description;
};

struct LegacyWebPluginInfo {
  // The path to the plugin file (DLL/bundle/library).
  base::FilePath path;

  // A list of all the mime types that this plugin supports.
  std::vector<LegacyWebPluginMimeType> mime_types;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_INFO_H_