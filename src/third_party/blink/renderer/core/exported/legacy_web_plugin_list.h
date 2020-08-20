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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_LIST_H_

#include "base/memory/singleton.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/renderer/core/exported/legacy_web_plugin_info.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class FilePath;
}

namespace blink {

class KURL;

class LegacyWebPluginList {
 public:
  static LegacyWebPluginList* GetInstance();
  bool GetPluginInfo(const KURL& url,
                     const std::string& mime_type,
                     LegacyWebPluginInfo& info,
                     std::string& actual_mime_type);
  bool IsSupportedMimeType(const WTF::String& mime_type);

 private:
  LegacyWebPluginList();
  void InitializePluginsInfo();
  void AddPlugin(const base::FilePath& filename,
                 const std::string& mime_description);
  bool SupportsType(const LegacyWebPluginInfo& plugin,
                    const std::string& mime_type);
  bool SupportsExtension(const LegacyWebPluginInfo& plugin,
                         const std::string& extension,
                         std::string& actual_mime_type);

  // Plugins
  std::vector<LegacyWebPluginInfo> plugins_;

  friend struct base::DefaultSingletonTraits<LegacyWebPluginList>;
  DISALLOW_COPY_AND_ASSIGN(LegacyWebPluginList);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_LEGACY_WEB_PLUGIN_LIST_H_