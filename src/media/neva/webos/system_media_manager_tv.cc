// Copyright 2018-2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/neva/webos/system_media_manager_tv.h"

#include <uMediaClient.h>
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
  return std::make_unique<SystemMediaManagerTv>(umedia_client,
                                                main_task_runner);
}

SystemMediaManagerTv::SystemMediaManagerTv(
    const base::WeakPtr<UMediaClientImpl>& umedia_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : SystemMediaManagerAcb(umedia_client, main_task_runner) {}

SystemMediaManagerTv::~SystemMediaManagerTv() {}

bool SystemMediaManagerTv::SendCustomMessage(const std::string& message) {
  Json::Reader reader;
  Json::FastWriter writer;
  Json::Value msg;

  if (!reader.parse(message, msg)) {
    LOG(ERROR) << __func__ << " json parse error";
    return false;
  }

  if (!msg.isObject())
    return false;

  if (!msg.isMember("command"))
    return false;

  if (!msg.isMember("parameter"))
    return false;

  if (msg["command"].asString() == "setSubWindowInfo") {
    std::string parameter = writer.write(msg["parameter"]);
    long res = acb_client_->setSubWindowInfo(parameter);
    LOG(INFO) << "setSubWindowInfo: parameter: " << parameter
              << ", res:" << res;
    return res == 1;
  } else if (msg["command"].asString() == "rotateVideoEx") {
    std::string parameter = writer.write(msg["parameter"]);
    long res = acb_client_->rotateVideoEx(parameter);
    LOG(INFO) << "rotateVideoEx: parameter: " << parameter << ", res:" << res;
    return res == 1;
  }
  return false;
}

void SystemMediaManagerTv::WillUpdateVideoInfo(
    Json::Value& root,
    const uMediaServer::video_info_t& video_info) {
  if (root.isMember("video"))
    root["video"]["rotation"] = std::to_string(video_info.rotation);
}

void SystemMediaManagerTv::WillUpdateAudioInfo(
    Json::Value& root,
    const uMediaServer::audio_info_t& audio_info) {}

void SystemMediaManagerTv::UpdateHtmlMediaOption(
    const Json::Value& html_media_option) {
  if (html_media_option.isMember("vsmInfo")) {
    if (html_media_option["vsmInfo"].isMember("sinkType"))
      media_option_.video_sink_type_ =
          html_media_option["vsmInfo"]["sinkType"].asString();
    if (html_media_option["vsmInfo"].isMember("dassAction"))
      media_option_.dass_action_ =
          html_media_option["vsmInfo"]["dassAction"].asString();
    if (html_media_option["vsmInfo"].isMember("useDassControl"))
      media_option_.use_dass_control_ =
          html_media_option["vsmInfo"]["useDassControl"].asBool();
    if (html_media_option["vsmInfo"].isMember("purpose"))
      media_option_.video_sink_purpose_ =
          html_media_option["vsmInfo"]["purpose"].asString();
  }
}

void SystemMediaManagerTv::UpdateMediaLoadInfo() {
  long sink_type = ACB::StateSinkType::MAIN;
  long dass_action = ACB::DassAction::AUTO;
  long sink_purpose = ACB::VsmPurpose::NONE;

  if (media_option_.video_sink_type_ == "MAIN")
    sink_type = ACB::StateSinkType::MAIN;
  else if (media_option_.video_sink_type_ == "SUB")
    sink_type = ACB::StateSinkType::SUB;

  if (media_option_.dass_action_ == "auto")
    dass_action = ACB::DassAction::AUTO;
  else if (media_option_.dass_action_ == "manual")
    dass_action = ACB::DassAction::MANUAL;

  if (media_option_.video_sink_purpose_ == "multiView")
    sink_purpose = ACB::VsmPurpose::MULTI_VIEW;
  else if (media_option_.video_sink_purpose_ == "liveZoom")
    sink_purpose = ACB::VsmPurpose::LIVE_ZOOM;

  FUNC_LOG(1) << __func__ << " call acb_client_->setVsmInfo(" << sink_type
              << ", " << dass_action << ", " << sink_purpose;
  acb_client_->setVsmInfo(sink_type, dass_action, sink_purpose);
}

long SystemMediaManagerTv::OverridePlayerType(long player_type) {
  return player_type;
}

void SystemMediaManagerTv::DoAudioMute(bool mute) {
  if (media_option_.use_dass_control_ == false)
    return;

  if (mute)
    acb_client_->disconnectDass();
  else
    acb_client_->connectDass();
}

void SystemMediaManagerTv::DoSetMediaAudioData(const std::string& param) {
  acb_client_->setMediaAudioData(param);
}
}  // namespace media
