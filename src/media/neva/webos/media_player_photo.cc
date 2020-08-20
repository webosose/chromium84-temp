// Copyright 2017-2020 LG Electronics, Inc.
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

#include "media/neva/webos/media_player_photo.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#define FUNC_LOG(x) DVLOG(x) << __PRETTY_FUNCTION__

namespace media {

#define BIND_TO_RENDER_LOOP(function)                   \
  (DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::BindRepeating(function, AsWeakPtr())))

MediaPlayerPhoto::MediaPlayerPhoto(
    MediaPlayerNevaClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const std::string& app_id)
    : client_(client), main_task_runner_(main_task_runner), app_id_(app_id) {
  FUNC_LOG(2);
#if defined(USE_ACB)
  acb_id_ = acb_client_->getAcbId();
#endif  // defined(USE_ACB)
}

MediaPlayerPhoto::~MediaPlayerPhoto() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);
  luna_service_client_.Unsubscribe(subscribe_key_);
  if (!pipeline_id_.empty()) {
#if defined(USE_ACB)
    LOG(INFO) << "ACB::setState(FOREGROUND, UNLOADED)";
    acb_client_->setState(ACB::AppState::FOREGROUND,
                          ACB::ExtInputState::UNLOADED);
    acb_state_ = ACB::PlayState::UNLOADED;
#endif  // defined(USE_ACB)

    ClosePipeline();
  }
}

void MediaPlayerPhoto::Initialize(const bool is_video,
                                  const double current_time,
                                  const std::string& url,
                                  const std::string& mime_type,
                                  const std::string& referrer,
                                  const std::string& user_agent,
                                  const std::string& cookies,
                                  const std::string& media_option,
                                  const std::string& custom_option) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " app_id: " << app_id_ << "url : " << url << " media_option - "
              << (media_option.empty() ? "{}" : media_option);

  url::Parsed parsed;
  url::ParseStandardURL(url.c_str(), url.length(), &parsed);
  url_ = GURL(url, parsed, true);
  mime_type_ = mime_type;

#if defined(USE_ACB)
  // Acb can callback after the instance destructed.
  // cb.Run won't callback to the instance
  // since chromium callback handles this situation with weakPtr.
  typedef base::Callback<void(long, long, long, long, long, const char*)>
      AcbHandlerCallback;
  AcbHandlerCallback cb =
      BIND_TO_RENDER_LOOP(&MediaPlayerPhoto::DispatchAcbHandler);
  auto fun = [](AcbHandlerCallback cb, long acb_id, long task_id,
                long event_type, long app_state, long play_state,
                const char* reply) {
    FUNC_LOG(2) << " called callback from acb";
    cb.Run(acb_id, task_id, event_type, app_state, play_state, reply);
  };
  if (!acb_client_->initialize(
          ACB::PlayerType::PHOTO, app_id_,
          std::bind(fun, cb, acb_id_, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5))) {
    LOG(ERROR) << " ACB::initialize failed";
  }
#endif  // defined(USE_ACB)

  Json::Value root;
  Json::FastWriter writer;

  if (mime_type_ == "service/webos-photo")
    root["type"] = "photo";
  else if (mime_type_ == "service/webos-photo-camera")
    root["type"] = "photo-camera";
  else
    LOG(ERROR) << " mime_type:" << mime_type << " Not supported";

  root["appId"] = app_id_;
#if defined(USE_GAV)
  root["windowId"] = media_layer_id_;
#endif  // defined(USE_GAV)

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::PHOTORENDERER, "open"),
      writer.write(root),
      BIND_TO_RENDER_LOOP(&MediaPlayerPhoto::HandleOpenReply));
}

void MediaPlayerPhoto::Start() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);

  if (is_suspended_)
    CallPhotoRendererResume();
#if defined(USE_ACB)
  else
    ChangeLoadedState();
#endif  // defined(USE_ACB)

  is_suspended_ = false;
}

void MediaPlayerPhoto::Pause() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);

  if (pipeline_id_.empty()) {
    LOG(INFO) << "pipeline_id_ is not available.";
    return;
  }

  is_suspended_ = true;
  if (pipeline_state_ != Paused)
    CallPhotoRendererPause();

#if defined(USE_ACB)
  ChangeSuspendState();
#endif  // defined(USE_ACB)
}

void MediaPlayerPhoto::SetRate(double playback_rate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " playback_rate =" << playback_rate;

  if (pipeline_id_.empty()) {
    if (playback_rate == 0.0f)
      is_suspended_ = true;
    playback_rate_ = playback_rate;
    return;
  }

  if (suspended_before_open_) {
    LOG(INFO) << " App was suspended before open";
    suspended_before_open_ = false;

    if (client_) {
      client_->OnMediaMetadataChanged(base::TimeDelta(), out_rect_.width(),
                                      out_rect_.height(), true);
    }
    return;
  }

  playback_rate_ = playback_rate;
}

void MediaPlayerPhoto::SetDisplayWindow(const gfx::Rect& out_rect,
                                        const gfx::Rect& in_rect,
                                        bool full_screen,
                                        bool forced) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " in_rect: " << in_rect.ToString()
              << " out_rect: " << out_rect.ToString()
              << " full_screen: " << full_screen << " forced:" << forced;

  full_screen_ = full_screen;
  if (out_rect.IsEmpty())
    return;

#if defined(USE_ACB)
  acb_client_->setDisplayWindow(out_rect.x(), out_rect.y(), out_rect.width(),
                                out_rect.height(), full_screen, 0);
#endif  // defined(USE_ACB)
  out_rect_ = out_rect;
}

void MediaPlayerPhoto::SetMediaLayerId(const std::string& media_layer_id) {
  media_layer_id_ = media_layer_id;
}

void MediaPlayerPhoto::CallPhotoRendererSubscribe() {
  FUNC_LOG(2);

  Json::Value root;
  root["pipelineID"] = pipeline_id_;
  root["subscribe"] = true;

  Json::FastWriter writer;

  luna_service_client_.Subscribe(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::PHOTORENDERER, "subscribe"),
      writer.write(root), &subscribe_key_,
      BIND_TO_RENDER_LOOP(&MediaPlayerPhoto::HandleSubscribeReply));
}

void MediaPlayerPhoto::CallPhotoRendererPlay() {
  FUNC_LOG(2);

  Json::Value root;
  root["pipelineID"] = pipeline_id_;
  Json::FastWriter writer;

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::PHOTORENDERER, "play"),
      writer.write(root));
}

void MediaPlayerPhoto::CallPhotoRendererPause() {
  FUNC_LOG(2);

  Json::Value root;
  root["pipelineID"] = pipeline_id_;

  Json::FastWriter writer;

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::PHOTORENDERER, "pause"),
      writer.write(root));
}

void MediaPlayerPhoto::CallPhotoRendererResume() {
  FUNC_LOG(2);

  Json::Value root;
  root["pipelineID"] = pipeline_id_;
  Json::FastWriter writer;

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::PHOTORENDERER, "resume"),
      writer.write(root),
      BIND_TO_RENDER_LOOP(&MediaPlayerPhoto::HandleResumeReply));
}

void MediaPlayerPhoto::ClosePipeline() {
  FUNC_LOG(2);

  Json::Value root;
  root["pipelineID"] = pipeline_id_;

  Json::FastWriter writer;

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::PHOTORENDERER, "close"),
      writer.write(root));
}

void MediaPlayerPhoto::HandleOpenReply(const std::string& payload) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " payload:" << payload.c_str();

  Json::Value response;
  Json::Reader reader;

  if (reader.parse(payload, response) && response.isMember("returnValue") &&
      response["returnValue"].asBool() && response.isMember("pipelineID")) {
    pipeline_id_ = response["pipelineID"].asString();
    pipeline_state_ = Open;
    CallPhotoRendererSubscribe();

#if defined(USE_ACB)
    if (!acb_client_->setMediaId(pipeline_id_.c_str()))
      LOG(ERROR) << "ACB::setMediaId failed";
#endif  // !defined(USE_ACB)
  } else {
    LOG(INFO) << "Failed to Open!!! ("
              << response["errorCode"].asString().c_str() << ","
              << response["errorText"].asString().c_str() << ")";
    response["infoType"] = "reload";

    Json::FastWriter writer;
    if (client_)
      client_->OnCustomMessage(media::MediaEventType::kMediaEventNone,
                               writer.write(response));
    return;
  }

  loaded_ = true;
  has_audio_ = true;
  has_video_ = true;

  if (is_suspended_) {
    LOG(INFO) << " App has been suspended before open";
    suspended_before_open_ = true;
    return;
  }

  if (client_) {
    client_->OnMediaMetadataChanged(base::TimeDelta(), out_rect_.width(),
                                    out_rect_.height(), true);
    client_->OnLoadComplete();
  }
}

void MediaPlayerPhoto::HandleSubscribeReply(const std::string& payload) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " payload:" << payload.c_str();

  Json::Value response;
  Json::Reader reader;

  if (!reader.parse(payload, response)) {
    LOG(ERROR) << " Json::Reader::parse (" << payload << ") - Failed!!!";
    return;
  }

  if (response.isMember("status")) {
    std::string status = response["status"].asString();
    if (status == "registered") {
      CallPhotoRendererPlay();
    } else if (status == "playing") {
      pipeline_state_ = Playing;

      if (!is_suspended_) {
        if (client_)
          client_->OnMediaMetadataChanged(base::TimeDelta(), out_rect_.width(),
                                          out_rect_.height(), true);
#if defined(USE_ACB)
        ChangePlayingState();
#endif
      }
    } else if (status == "paused") {
#if defined(USE_ACB)
      if (acb_state_ == ACB::PlayState::PLAYING)
        ChangeSuspendState();
#endif  // defined(USE_ACB)

      pipeline_state_ = Paused;
    }
  }

  if (response.isMember("sinkInfo")) {
    std::string pattern = response["sinkInfo"]["pattern"].asString();
    gfx::Rect in_rect(
        response["sinkInfo"]["drawRect"]["sink"]["input"]["x"].asInt(),
        response["sinkInfo"]["drawRect"]["sink"]["input"]["y"].asInt(),
        response["sinkInfo"]["drawRect"]["sink"]["input"]["w"].asInt(),
        response["sinkInfo"]["drawRect"]["sink"]["input"]["h"].asInt());
    gfx::Rect out_rect(
        response["sinkInfo"]["drawRect"]["sink"]["output"]["x"].asInt(),
        response["sinkInfo"]["drawRect"]["sink"]["output"]["y"].asInt(),
        response["sinkInfo"]["drawRect"]["sink"]["output"]["w"].asInt(),
        response["sinkInfo"]["drawRect"]["sink"]["output"]["h"].asInt());

    SetDisplayWindow(in_rect, out_rect, pattern);
  }
}

void MediaPlayerPhoto::HandleResumeReply(const std::string& payload) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " payload:" << payload.c_str();

  if (is_suspended_) {
    LOG(INFO) << "App has been backgrounded";
    return;
  }

  Json::Value response;
  Json::Reader reader;

  if (reader.parse(payload, response) && response.isMember("returnValue") &&
      response["returnValue"].asBool()) {
    pipeline_state_ = Open;
#if defined(USE_ACB)
    ChangeLoadedState();
#endif  // defined(USE_ACB)
  }
}

#if defined(USE_ACB)
void MediaPlayerPhoto::ChangePlayingState() {
  FUNC_LOG(2);
  if (!acb_client_->setState(ACB::AppState::FOREGROUND,
                             ACB::PlayState::PLAYING)) {
    LOG(ERROR) << " ACB::setState(FOREGROUND, PLAYING) taskId : "
               << acb_load_task_id_;
  }
  acb_state_ = ACB::PlayState::PLAYING;
}

void MediaPlayerPhoto::ChangeLoadedState() {
  FUNC_LOG(2);
  if (!acb_client_->setState(ACB::AppState::FOREGROUND, ACB::PlayState::LOADED,
                             &acb_load_task_id_)) {
    LOG(ERROR) << " ACB::setState(FOREGROUND, LOADED)";
  }
  acb_state_ = ACB::PlayState::LOADED;
}

void MediaPlayerPhoto::ChangeSuspendState() {
  FUNC_LOG(2);
  if (!acb_client_->setState(ACB::AppState::BACKGROUND,
                             ACB::PlayState::LOADED)) {
    LOG(ERROR) << "Failed ACB::setState(BACKGROUND, LOADED) taskId : "
               << acb_load_task_id_;
  }
  acb_state_ = ACB::PlayState::LOADED;
}

void MediaPlayerPhoto::SetMediaVideoData(const gfx::Rect& in_rect,
                                         const std::string& pattern) {
  Json::Value root;
  Json::Value video;
  root["context"] = pipeline_id_;
  if (mime_type_ == "service/webos-photo")
    root["content"] = "photo";
  else if (mime_type_ == "service/webos-photo-camera")
    root["content"] = "photo-camera";
  else
    LOG(ERROR) << " mime_type: " << mime_type_ << " : should not use this";

  video["frameRate"] = 60;
  video["scanType"] = "VIDEO_PROGRESSIVE";
  video["width"] = in_rect.width();
  video["height"] = in_rect.height();

  video["pixelAspectRatio"]["width"] = 1;
  video["pixelAspectRatio"]["height"] = 1;

  video["data3D"]["currentPattern"] = pattern;
  video["data3D"]["typeLR"] = "LR";
  video["data3D"]["originalPattern"] = pattern;

  root["video"] = video;

  Json::FastWriter writer;
  std::string parameter = writer.write(root);
  LOG(INFO) << "ACB::setMediaVideoData( " << parameter.c_str() << " )";

  if (!acb_client_->setMediaVideoData(parameter.c_str()))
    LOG(ERROR) << "ACB::setMediaVideoData failed";
}
#endif  // defined(USE_ACB)

void MediaPlayerPhoto::SetDisplayWindow(const gfx::Rect& in_rect,
                                        const gfx::Rect& out_rect,
                                        const std::string& pattern) {
#if defined(USE_ACB)
  SetMediaVideoData(out_rect, pattern);

  if (out_rect == in_rect) {
    gfx::Rect print_rect = out_rect;
    LOG(INFO) << "setDisplayWindow: out_rect: " << print_rect.ToString();
    acb_client_->setDisplayWindow(out_rect.x(), out_rect.y(), out_rect.width(),
                                  out_rect.height(), true, 0);
  } else {
    LOG(INFO) << "setCustomDisplayWindow: "
              << "in_rect: " << in_rect.ToString() << " / "
              << "out_rect_: " << out_rect_.ToString();
    acb_client_->setCustomDisplayWindow(
        in_rect.x(), in_rect.y(), in_rect.width(), in_rect.height(),
        out_rect_.x(), out_rect_.y(), out_rect_.width(), out_rect_.height(),
        true, 0);
  }
#endif  // defined(USE_ACB)
}

#if defined(USE_ACB)
void MediaPlayerPhoto::DispatchAcbHandler(long acb_id,
                                          long task_id,
                                          long event_type,
                                          long app_state,
                                          long play_state,
                                          const char* reply) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " acb_id(" << acb_id << ") taskId(" << task_id << ")"
              << " appState(" << app_state << ") playState(" << play_state
              << ") reply(" << (reply ? reply : " ") << ")";

  base::AutoLock auto_lock(lock_);

  if (acb_id_ != acb_id) {
    LOG(ERROR) << "wrong acbId. Ignore this message";
    return;
  }

  if (task_id == acb_load_task_id_) {
    acb_load_task_id_ = 0;
    AcbLoadCompleted();
  }
}

void MediaPlayerPhoto::AcbLoadCompleted() {
  FUNC_LOG(2);

  if (is_suspended_) {
    LOG(INFO) << "App has been backgrounded";
    return;
  }

  pipeline_state_ = Open;
  CallPhotoRendererPlay();
  if (!(out_rect_.IsEmpty()))
    acb_client_->setDisplayWindow(out_rect_.x(), out_rect_.y(),
                                  out_rect_.width(), out_rect_.height(),
                                  full_screen_, 0);
}
#endif  // defined(USE_ACB)

}  // namespace media
