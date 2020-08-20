// Copyright 2018-2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_TV_H_
#define MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_TV_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "media/neva/webos/system_media_manager_acb.h"

namespace media {
class SystemMediaManagerTv : public SystemMediaManagerAcb {
 public:
  SystemMediaManagerTv(
      const base::WeakPtr<UMediaClientImpl>& umedia_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  ~SystemMediaManagerTv();

  bool SendCustomMessage(const std::string& message) override;

 private:
  struct MediaOptions {
    std::string media_transport_type_;
    std::string video_sink_type_;
    std::string video_sink_purpose_;
    std::string dass_action_;
    bool use_dass_control_;
  };

  void WillUpdateVideoInfo(
      Json::Value& root,
      const uMediaServer::video_info_t& video_info) override;
  void WillUpdateAudioInfo(
      Json::Value& root,
      const uMediaServer::audio_info_t& audio_info) override;
  void UpdateHtmlMediaOption(const Json::Value& html_media_option) override;
  void UpdateMediaLoadInfo() override;
  long OverridePlayerType(long player_type) override;
  void DoAudioMute(bool mute) override;
  void DoSetMediaAudioData(const std::string& param) override;

  struct MediaOptions media_option_;
};

}  // namespace media
#endif  // MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_TV_H_
