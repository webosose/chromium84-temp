// Copyright 2018-2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/neva/webos/system_media_manager_signage.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/neva/webos/media_util.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#define FUNC_LOG(x) DVLOG(x) << __func__

namespace media {

std::unique_ptr<SystemMediaManager> SystemMediaManager::Create(
    const base::WeakPtr<UMediaClientImpl>& umedia_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  return std::make_unique<SystemMediaManagerSignage>(umedia_client,
                                                     main_task_runner);
}

SystemMediaManagerSignage::SystemMediaManagerSignage(
    const base::WeakPtr<UMediaClientImpl>& umedia_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : SystemMediaManagerAcb(umedia_client, main_task_runner) {}

SystemMediaManagerSignage::~SystemMediaManagerSignage() {}

void SystemMediaManagerSignage::EofReceived() {
  // on M16pp, playlist Gapless case, after each End of File, VSM needs to
  // re-connect
  if (media_transport_type_ == "GAPLESS") {
    acb_client_->setVideoSink("gapless");
  }
}

void SystemMediaManagerSignage::WillSetAcbState(AppState& app,
                                                PlayState& play) {
  if (media_transport_type_ == "RTP" &&
      CurrentPlayState() == PlayState::kUnloaded) {
    app = AppState::kForeground;
    play = PlayState::kLoaded;
  }
}

void SystemMediaManagerSignage::WillUpdateVideoInfo(
    Json::Value& root,
    const uMediaServer::video_info_t& video_info) {
  if (media_transport_type_ == "RTSP")
    SetAcbStateInternal(AppState::kForeground, PlayState::kLoaded);

  if (media_transport_type_ == "UDP" || media_transport_type_ == "RTP") {
    if (CurrentPlayState() == PlayState::kUnloaded)
      SetAcbStateInternal(AppState::kForeground, PlayState::kLoaded);
  }
}

void SystemMediaManagerSignage::WillUpdateAudioInfo(
    Json::Value& root,
    const uMediaServer::audio_info_t& audio_info) {}

void SystemMediaManagerSignage::UpdateMediaOption(
    const Json::Value& media_option) {
  SystemMediaManagerAcb::UpdateMediaOption(media_option);
  if (media_transport_type_ == "GAPLESS")
    video_purpose_ = VideoPurpose::kGapless;
}

void SystemMediaManagerSignage::UpdateHtmlMediaOption(
    const Json::Value& html_media_option) {}

void SystemMediaManagerSignage::UpdateMediaLoadInfo() {
  acb_client_->setAVInfo(has_video_, has_audio_);
  acb_client_->setVideoSink(VideoPurposeToString(video_purpose_));

  if (media_transport_type_ == "RTSP") {
    // state has to be set to Loaded and AppState should be Foreground, for
    // video rendering in RTSP playback.
    VLOG(2) << "setting Acb State LOADED: RTSP Case";
    SetAcbStateInternal(AppState::kForeground, PlayState::kLoaded);
  }
}

long SystemMediaManagerSignage::OverridePlayerType(long player_type) {
  return player_type;
}

void SystemMediaManagerSignage::DoAudioMute(bool mute) {
  acb_client_->setVolumeMute(mute);
}

void SystemMediaManagerSignage::DidInitialize() {
  // TODO: setVideoRenderMode API was originally made for Quad mode/VTV
  // After those features are enabled, we have to modify it for proper work.
  bool is_quad_mode = false;
  bool is_vtv = false;
  acb_client_->setVideoRenderMode(is_quad_mode, is_vtv);

  auto func = [](ActiveRegionCB cb, ACB::WindowRect rect) {
    cb.Run(gfx::Rect(rect.left_, rect.top_, rect.width_, rect.height_));
  };

  acb_client_->setActiveRegionCallback(
      std::bind(func, active_region_cb_, std::placeholders::_1));
}

std::string SystemMediaManagerSignage::VideoPurposeToString(
    VideoPurpose purpose) {
  switch (purpose) {
    case VideoPurpose::kGapless:
      return "gapless";
    default:
      return "none";
  }
}

}  // namespace media
