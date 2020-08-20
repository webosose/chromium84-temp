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

#include "legacy_web_plugin_list.h"

#include "base/files/file_path.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

LegacyWebPluginList* LegacyWebPluginList::GetInstance() {
  return base::Singleton<LegacyWebPluginList>::get();
}

bool LegacyWebPluginList::GetPluginInfo(const KURL& url,
                                        const std::string& mime_type,
                                        LegacyWebPluginInfo& info,
                                        std::string& actual_mime_type) {
  DCHECK(mime_type == base::ToLowerASCII(mime_type));

  // Add in plugins by mime type.
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsType(plugins_[i], mime_type)) {
      info = plugins_[i];
      actual_mime_type = mime_type;
      return true;
    }
  }

  // Add in plugins by url.
  // We do not permit URL-sniff based plugin MIME type overrides aside from
  // the case where the "type" was initially missing.
  // We collected stats to determine this approach isn't a major compat issue,
  // and we defend against content confusion attacks in various cases, such
  // as when the user doesn't have the Flash plugin enabled.
  std::string path = url.GetPath().Utf8().data();
  std::string::size_type last_dot = path.rfind('.');
#if defined(OS_WEBOS)
  if (last_dot != std::string::npos) {
#else
  if (last_dot != std::string::npos && mime_type.empty()) {
#endif
    std::string extension =
        base::ToLowerASCII(base::StringPiece(path).substr(last_dot + 1));
    std::string actual_mime_type;
    for (size_t i = 0; i < plugins_.size(); ++i) {
      if (SupportsExtension(plugins_[i], extension, actual_mime_type)) {
        info = plugins_[i];
        return true;
      }
    }
  }
  return false;
}

bool LegacyWebPluginList::IsSupportedMimeType(const WTF::String& mime_type) {
  std::string mime_type_utf8 = WebString(mime_type).Utf8();
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsType(plugins_[i], mime_type_utf8)) {
      return true;
    }
  }
  return false;
}

LegacyWebPluginList::LegacyWebPluginList() {
  InitializePluginsInfo();
}

void LegacyWebPluginList::InitializePluginsInfo() {
  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libAdvertisementPlugin.so")),
            "application/x-netcast-ad::;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libBroadCastPlugin.so")),
            "application/x-netcast-broadcast::;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libDevicePlugin.so")),
            "application/x-netcast-info::;"
            "systeminfoobject::;"
            "lanconfigobject::;"
            "wlanconfigobject::;"
            "application/x-netcast-config::;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libDrmAgentPlugin.so")),
            "application/oipfDrmAgent::;"
            "application/x-netcast-securemedia::;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libMediaPlugin.so")),
            "audio/x-ms-wma:wma:;"
            "video/x-ms-wmv:wmv:;"
            "video/x-ms-asf:asf:;"
            "application/x-ms-wmp:wmp:;"
            "application/x-mplayer2::;"
            "video/vnd.ms-playready.media.pyv:pyv:;"
            "video/vnd.ms-playready.media.pya:pya:;"
            "video/mp4:mp4:;"
            "video/x-m4v:m4v:;"
            "video/mpeg:mpg:;"
            "video/x-msvideo:avi:;"
            "audio/mp3:mp3:;"
            "audio/mp4:mp4:;"
            "audio/mpeg:mpeg:;"
            "video/x-flv:x-flv:;"
            "video/x-f4v:f4v:;"
            "video/x-ms-ism:ism:;"
            "application/vnd.ms-sstr+xml:ism:;"
            "application/vnd.apple.mpegurl:m3u8:;"
            "application/vnd.apple.mpegurl.audio:m3u8:;"
            "application/dash+xml:mpd:;"
            "application/x-netcast-av::;"
            "application/x-netcast::;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libServicePlugin.so")),
            "application/x-netcast-service::;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libSoundPlugin.so")),
            "audio/wav:wav:;"
            "audio/x-wav:wav:;");

  AddPlugin(base::FilePath(FILE_PATH_LITERAL("libHbbtv2Plugin.so")),
            "application/oipfConfiguration::HBBTV JS Plug-in for Configuration;"
            "application/oipfApplicationManager::HBBTV JS Plug-in for "
            "Application Manager;"
            "application/oipfCapabilities::HBBTV JS Plug-in for Capabilities;"
            "application/vnd.lge.hbbtv::HBBTV JS Plug-in for LG Specific "
            "application manager;"
            "application/oipfdrmagent::HBBTV JS Plug-in for DRMAgent;"
            "video/broadcast::HBBTV JS Plug-in for VideoBroadcast;"
            "audio/mpeg:mp3:HBBTV JS Plug-in for AVControl;"
            "audio/mp4:mp4:HBBTV JS Plug-in for AVControl;"
            "video/mp4:mp4:HBBTV JS Plug-in for AVControl;"
            "video/mpeg:mpg:HBBTV JS Plug-in for AVControl;"
            "video/mpeg4:mp4:HBBTV JS Plug-in for AVControl;"
            "video/mp2t:ts:HBBTV JS Plug-in for AVControl;"
            "video/x-ms-ism:ism:HBBTV JS Plug-in for AVControl;"
            "application/vnd.ms-sstr+xml:ism:HBBTV JS Plug-in for AVControl;"
            "application/vnd.apple.mpegurl:m3u8:HBBTV JS Plug-in for AVControl;"
            "audio/mpegurl:m3u8:HBBTV JS Plug-in for AVControl;"
            "application/dash+xml:xml:HBBTV JS Plug-in for AVControl;"
            "application/vnd.oipf.ContentAccessStreaming+xml:xml:HBBTV JS "
            "Plug-in for AVControl;"
            "application/oipfSearchManager::HBBTV JS Plug-in for SearchManager;"
#ifdef CS_COMPILATION_ENABLE
            "application/hbbtvCSManager::HBBTV JS Plug-in for Companion Screen "
            "Manager;"
#endif
            "application/oipfParentalControlManager::HBBTV JS Plug-in for "
            "OIPFControlManager;");
}

static void ParseMIMEDescription(
    const std::string& description,
    std::vector<LegacyWebPluginMimeType>* mime_types) {
  // We parse the description here into WebPluginMimeType structures.
  // Naively from the NPAPI docs you'd think you could use
  // string-splitting, but the Firefox parser turns out to do something
  // different: find the first colon, then the second, then a semi.
  //
  // See ParsePluginMimeDescription near
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUtils.h#53

  std::string::size_type ofs = 0;
  for (;;) {
    LegacyWebPluginMimeType mime_type;

    std::string::size_type end = description.find(':', ofs);
    if (end == std::string::npos)
      break;
    mime_type.mime_type =
        base::ToLowerASCII(description.substr(ofs, end - ofs));
    ofs = end + 1;

    end = description.find(':', ofs);
    if (end == std::string::npos)
      break;
    const std::string extensions = description.substr(ofs, end - ofs);
    mime_type.file_extensions = base::SplitString(
        extensions, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ofs = end + 1;

    end = description.find(';', ofs);
    // It's ok for end to run off the string here.  If there's no
    // trailing semicolon we consume the remainder of the string.
    if (end != std::string::npos) {
      mime_type.description =
          base::UTF8ToUTF16(description.substr(ofs, end - ofs));
    } else {
      mime_type.description = base::UTF8ToUTF16(description.substr(ofs));
    }
    mime_types->push_back(mime_type);
    if (end == std::string::npos)
      break;
    ofs = end + 1;
  }
}

void LegacyWebPluginList::AddPlugin(const base::FilePath& filename,
                                    const std::string& mime_description) {
  if (filename.empty() || mime_description.empty()) {
    return;
  }
  LegacyWebPluginInfo info;
  info.path = filename;
  ParseMIMEDescription(mime_description, &info.mime_types);
  plugins_.push_back(info);
}

bool LegacyWebPluginList::SupportsType(const LegacyWebPluginInfo& plugin,
                                       const std::string& mime_type) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const LegacyWebPluginMimeType& mime_info = plugin.mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (mime_info.mime_type == "*")
        continue;
      return true;
    }
  }
  return false;
}

bool LegacyWebPluginList::SupportsExtension(const LegacyWebPluginInfo& plugin,
                                            const std::string& extension,
                                            std::string& actual_mime_type) {
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const LegacyWebPluginMimeType& mime_type = plugin.mime_types[i];
    for (size_t j = 0; j < mime_type.file_extensions.size(); ++j) {
      if (mime_type.file_extensions[j] == extension) {
        actual_mime_type = mime_type.mime_type;
        return true;
      }
    }
  }
  return false;
}

}  // namespace blink
