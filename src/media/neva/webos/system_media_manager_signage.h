// Copyright 2018-2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_SIGNAGE_H_
#define MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_SIGNAGE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "media/neva/webos/system_media_manager_acb.h"

namespace media {

class SystemMediaManagerSignage : public SystemMediaManagerAcb {
 public:
  SystemMediaManagerSignage(
      const base::WeakPtr<UMediaClientImpl>& umedia_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  ~SystemMediaManagerSignage();

  void EofReceived() override;

 private:
  struct MediaOptions {
    std::string media_transport_type_;
  };

  enum class VideoPurpose {
    kNone,
    kGapless,
  };

  void WillSetAcbState(AppState& app, PlayState& play) override;
  void WillUpdateVideoInfo(
      Json::Value& root,
      const uMediaServer::video_info_t& video_info) override;
  void WillUpdateAudioInfo(
      Json::Value& root,
      const uMediaServer::audio_info_t& audio_info) override;
  void UpdateMediaOption(const Json::Value& media_option) override;
  void UpdateHtmlMediaOption(const Json::Value& html_media_option) override;
  void UpdateMediaLoadInfo() override;
  long OverridePlayerType(long player_type) override;
  void DoAudioMute(bool mute) override;
  void DidInitialize() override;

  std::string VideoPurposeToString(VideoPurpose purpose);

  struct MediaOptions media_options_;
  VideoPurpose video_purpose_ = VideoPurpose::kNone;
};

}  // namespace media

#endif  // MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_SIGNAGE_H_
