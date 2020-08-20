// Copyright 2015-2020 LG Electronics, Inc.
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

#include "media/neva/webos/media_player_extinput.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#define FUNC_LOG(x) DVLOG(x) << __func__

namespace {

#if defined(USE_GAV)
#define INPUT_PREFIX ""
const char kPipelineId[] = "pipelineId";
#else
#define INPUT_PREFIX "input/"
const char kPipelineId[] = "externalInputId";
#endif

const char kExternalDeviceOpen[] = INPUT_PREFIX "open";
const char kExternalDevicePause[] = INPUT_PREFIX "pause";
const char kExternalDeviceSwitch[] = INPUT_PREFIX "switchExternalInput";
const char kExternalDeviceStop[] = INPUT_PREFIX "stop";
const char kExternalDeviceClose[] = INPUT_PREFIX "close";
const char kDisplayGetCurrentVideo[] = "getCurrentVideo";

}  // namespace

namespace media {

static int64_t gExternalInputDeviceCounter = 0;

#define BIND_TO_RENDER_LOOP(function)                   \
  (DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::BindRepeating(function, AsWeakPtr())))

MediaPlayerExtInput::MediaPlayerExtInput(
    MediaPlayerNevaClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const std::string& app_id)
    : client_(client), main_task_runner_(main_task_runner), app_id_(app_id) {
#if defined(USE_ACB)
  acb_id_ = acb_client_->getAcbId();
#endif  // defined(USE_ACB)
}

MediaPlayerExtInput::~MediaPlayerExtInput() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);
  luna_service_client_.Unsubscribe(subscribe_key_);
  if (!external_input_id_.empty()) {
#if defined(USE_ACB)
    LOG(INFO) << "ACB::setState(FOREGROUND, UNLOADED)";
    acb_client_->setState(ACB::AppState::FOREGROUND,
                          ACB::ExtInputState::UNLOADED);
#endif  // defined(USE_ACB)
    ClosePipeline();
  }
}

void MediaPlayerExtInput::Initialize(const bool is_video,
                                     const double current_time,
                                     const std::string& url,
                                     const std::string& mime_type,
                                     const std::string& referrer,
                                     const std::string& user_agent,
                                     const std::string& cookies,
                                     const std::string& media_option,
                                     const std::string& custom_option) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << "app_id: " << app_id_ << "url : " << url << "media_option - "
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
      BIND_TO_RENDER_LOOP(&MediaPlayerExtInput::DispatchAcbHandler);
  auto fun = [](AcbHandlerCallback cb, long acb_id, long task_id,
                long event_type, long app_state, long play_state,
                const char* reply) {
    FUNC_LOG(2) << " called callback from acb";
    cb.Run(acb_id, task_id, event_type, app_state, play_state, reply);
  };
  if (!acb_client_->initialize(
          ACB::PlayerType::EXT_INPUT, app_id_,
          std::bind(fun, cb, acb_id_, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5)))
    LOG(ERROR) << ("ACB::initialize failed");

// TODO: setVideoRenderMode API was originally made for Quad mode/VTV.
// After those features are enabled, we have to modify it for proper work.
#if defined(USE_SIGNAGE_MEDIA)
  bool is_quad_mode = false;
  bool is_vtv = false;
  acb_client_->setVideoRenderMode(is_quad_mode, is_vtv);
#endif
#endif  // defined(USE_ACB)

  Json::Value root;
  Json::FastWriter writer;

#if defined(USE_GAV)
  Json::Value options;
  options["appId"] = app_id_;
  options["windowId"] = media_layer_id_;
  root["options"] = options;
  std::string host = url_.host();
  std::transform(host.begin(), host.end(), host.begin(), ::toupper);
  root["inputSourceType"] = host;
#endif  // defined(USE_GAV)

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::EXTERNALDEVICE,
          kExternalDeviceOpen),
      writer.write(root),
      BIND_TO_RENDER_LOOP(&MediaPlayerExtInput::HandleOpenReply));
}

void MediaPlayerExtInput::HandleOpenReply(const std::string& payload) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " payload:" << payload.c_str();

  Json::Value response;
  Json::Reader reader;

  if (reader.parse(payload, response) && response.isMember("returnValue") &&
      response["returnValue"].asBool() && response.isMember(kPipelineId)) {
    external_input_id_ = response[kPipelineId].asString();
#if defined(USE_ACB)
#if defined(USE_SIGNAGE_MEDIA)
    acb_client_->setVideoSink();  // to support 2video with extrnal inputs
#endif
    LOG(INFO) << "ACB::setMediaId(" << external_input_id_.c_str() << ")";
    if (!acb_client_->setMediaId(external_input_id_.c_str()))
      LOG(ERROR) << "ACB::setMediaId failed";
#endif  // defined(USE_ACB)
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

  if (client_) {
    client_->OnMediaMetadataChanged(base::TimeDelta(), source_size_.width(),
                                    source_size_.height(), true);
    client_->OnLoadComplete();
  }
}

void MediaPlayerExtInput::Start() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " playpack_reate:" << playback_rate_;
  if (playback_rate_ == 0.0f)
    return;
#if defined(USE_GAV)
  CallswitchExternalInput();
#else
  GetCurrentVideo();
  if (acb_client_->setState(ACB::AppState::FOREGROUND,
                            ACB::ExtInputState::LOADED, &acb_load_task_id_))
    LOG(INFO) << "taskId : " << acb_load_task_id_;
#endif  // defined(USE_GAV)
}

void MediaPlayerExtInput::Pause() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);
  if (external_input_id_.empty()) {
    LOG(ERROR) << "external_input_id_ is not available.";
    return;
  }
#if defined(USE_ACB)
  acb_client_->setState(ACB::AppState::BACKGROUND,
                        ACB::ExtInputState::SUSPENDED);
#endif  // defined(USE_ACB)

  Json::Value root;
  root[kPipelineId] = external_input_id_;
  Json::FastWriter writer;

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::EXTERNALDEVICE,
          kExternalDevicePause),
      writer.write(root));
}

void MediaPlayerExtInput::SetVolume(double volume) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (external_input_id_.empty())
    return;

#if defined(USE_ACB)
// We don't have volume level control API in here. Only audio mute is
// supported. Restrictions:
// 1. TV media doesn't support setVolumeMute() API.
// 2. Signage media's start/stop audio mute API doesn't work as expected.
#if defined(USE_SIGNAGE_MEDIA)
  acb_client_->setVolumeMute(volume == 0.0f);
#else
  if (volume == 0.0f)
    acb_client_->startMute(true, false);
  else
    acb_client_->stopMute(true, false);
#endif
#endif  // defined(USE_ACB)
}

void MediaPlayerExtInput::SetRate(double playback_rate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);
  playback_rate_ = playback_rate;
}

void MediaPlayerExtInput::SetDisplayWindow(const gfx::Rect& out_rect,
                                           const gfx::Rect& in_rect,
                                           bool full_screen,
                                           bool forced) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << "in_rect: " << in_rect.ToString()
              << "out_rect: " << out_rect.ToString()
              << "full_screen: " << full_screen << " forced:" << forced;

  out_rect_ = out_rect;
  in_rect_ = in_rect;
  full_screen_ = full_screen;

#if defined(USE_ACB)
  if (in_rect_.IsEmpty()) {
    acb_client_->setDisplayWindow(out_rect.x(), out_rect.y(), out_rect.width(),
                                  out_rect.height(), full_screen_, 0);
  } else {
    acb_client_->setCustomDisplayWindow(
        in_rect_.x(), in_rect_.y(), in_rect_.width(), in_rect_.height(),
        out_rect_.x(), out_rect_.y(), out_rect_.width(), out_rect_.height(),
        full_screen_, 0);
  }
#endif  // defined(USE_ACB)
}

bool MediaPlayerExtInput::UsesIntrinsicSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if defined(USE_TV_MEDIA)
  return false;
#else
  return true;
#endif
}

void MediaPlayerExtInput::SetDisableAudio(bool disable) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  audio_disabled_ = disable;
}

void MediaPlayerExtInput::SetMediaLayerId(const std::string& media_layer_id) {
  media_layer_id_ = media_layer_id;
}

#if defined(USE_ACB)
void MediaPlayerExtInput::AcbLoadCompleted() {
  FUNC_LOG(2);
  CallswitchExternalInput();
  SetDisplayWindow(out_rect_, in_rect_, full_screen_);
}
#endif  // defined(USE_ACB)

void MediaPlayerExtInput::CallswitchExternalInput() {
  FUNC_LOG(2);

  Json::Value root;
  root[kPipelineId] = external_input_id_;
#if defined(USE_GAV)
  unsigned port;
  if (!base::StringToUint(url_.port(), &port)) {
    LOG(ERROR) << __func__ << " Invalid port=" << url_.port();
    return;
  }
  root["portId"] = port;
#else
  std::string host = url_.host();
  std::transform(host.begin(), host.end(), host.begin(), ::toupper);
  root["inputSourceType"] = host;
  root["portId"] = url_.port();
  root["uniqueId"] = std::to_string(gExternalInputDeviceCounter++);
  root["needAudio"] = !audio_disabled_;
#endif  // defined(USE_GAV)

  Json::FastWriter writer;
  std::string parameter = writer.write(root);

  std::string uri = base::LunaServiceClient::GetServiceURI(
      base::LunaServiceClient::URIType::EXTERNALDEVICE, kExternalDeviceSwitch);
  LOG(INFO) << "url: " << url_.spec();
  LOG(INFO) << "service-uri: " << uri;
  LOG(INFO) << "param: " << parameter;

  luna_service_client_.CallAsync(
      uri, parameter,
      BIND_TO_RENDER_LOOP(
          &MediaPlayerExtInput::HandleSwitchExternalInputReply));
}

void MediaPlayerExtInput::HandleSwitchExternalInputReply(
    const std::string& payload) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2);

  Json::Value response;
  Json::Reader reader;

  if (reader.parse(payload, response) && response.isMember("returnValue") &&
      response["returnValue"].asBool()) {
    if (client_)
      client_->OnCustomMessage(
          media::MediaEventType::kMediaEventPipelineStarted, "");

#if defined(USE_ACB)
    LOG(INFO) << "ACB::setState(FOREGROUND, PLAYING)";
    acb_client_->setState(ACB::AppState::FOREGROUND,
                          ACB::ExtInputState::PLAYING);
#endif  // defined(USE_ACB)
  } else {
    LOG(ERROR) << "Failed to switch to external input!!! ("
               << response["errorCode"].asString().c_str() << ","
               << response["errorText"].asString().c_str() << ")";
    response["infoType"] = "switchExternalInput";

    Json::FastWriter writer;
    if (client_)
      client_->OnCustomMessage(media::MediaEventType::kMediaEventNone,
                               writer.write(response));
  }
}

void MediaPlayerExtInput::ClosePipeline() {
  FUNC_LOG(2);
  Json::Value root;
  root[kPipelineId] = external_input_id_;

  Json::FastWriter writer;

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::EXTERNALDEVICE,
          kExternalDeviceStop),
      writer.write(root));

  luna_service_client_.CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::EXTERNALDEVICE,
          kExternalDeviceClose),
      writer.write(root));
}

void MediaPlayerExtInput::GetCurrentVideo() {
  FUNC_LOG(2);
  Json::Value root;
  root["broadcastId"] = external_input_id_;
  root["subscribe"] = true;

  Json::FastWriter writer;
  luna_service_client_.Subscribe(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::DISPLAY, kDisplayGetCurrentVideo),
      writer.write(root), &subscribe_key_,
      BIND_TO_RENDER_LOOP(&MediaPlayerExtInput::HandleGetCurrentVideoReply));
}

void MediaPlayerExtInput::HandleGetCurrentVideoReply(
    const std::string& payload) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " payload:" << payload.c_str();
  Json::Value response;
  Json::Reader reader;

  if (reader.parse(payload, response) && response.isMember("returnValue") &&
      response["returnValue"].asBool() && response.isMember("width") &&
      response.isMember("height")) {
    gfx::Size source_size(response["width"].asInt(),
                          response["height"].asInt());
    if (!source_size.IsEmpty() && source_size_ != source_size) {
      source_size_ = source_size;
      client_->OnVideoSizeChanged(source_size_.width(), source_size_.height());
    }
  }
}

#if defined(USE_ACB)
void MediaPlayerExtInput::DispatchAcbHandler(long acb_id,
                                             long task_id,
                                             long event_type,
                                             long app_state,
                                             long play_state,
                                             const char* reply) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(2) << " adbId(" << acb_id << ") taskId(" << task_id << ") eventType("
              << event_type << ") appState(" << app_state << ") playState("
              << play_state << ") reply(" << reply << ")";
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
#endif  // defined(USE_ACB)

}  // namespace media
