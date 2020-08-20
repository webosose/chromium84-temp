// Copyright 2018-2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/neva/webos/system_media_manager_acb.h"

#include <Acb.h>
#include <uMediaClient.h>
#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/neva/webos/media_util.h"
#include "media/neva/webos/umediaclient_impl.h"
#include "media/neva/webos/weak_function.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#define FUNC_LOG(x) DVLOG(x) << __func__

namespace {
using media::SystemMediaManager;

const char kLiveDmost[] = "com.webos.app.livedmost";
const char kRecordings[] = "com.webos.app.recordings";

const char* AppStateToString(SystemMediaManager::AppState s) {
  switch (s) {
    case SystemMediaManager::AppState::kInit:
      return "Init";
    case SystemMediaManager::AppState::kBackground:
      return "Background";
    case SystemMediaManager::AppState::kForeground:
      return "Foreground";
    default:
      return "Unknown";
  }
}

const char* PlayStateToString(SystemMediaManager::PlayState s) {
  switch (s) {
    case SystemMediaManager::PlayState::kUnloaded:
      return "Unloaded";
    case SystemMediaManager::PlayState::kLoaded:
      return "Loaded";
    case SystemMediaManager::PlayState::kPlaying:
      return "Playing";
    case SystemMediaManager::PlayState::kPaused:
      return "Paused";
    default:
      return "Unknown";
  }
}
}  // namespace

namespace media {
SystemMediaManagerAcb::SystemMediaManagerAcb(
    const base::WeakPtr<UMediaClientImpl>& umedia_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : umedia_client_(umedia_client), main_task_runner_(main_task_runner) {
  acb_id_ = acb_client_->getAcbId();

  event_handler_ = std::bind(
      neva::BindToLoop(main_task_runner_, &SystemMediaManagerAcb::AcbHandler,
                       weak_factory_.GetWeakPtr()),
      acb_id_, std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

  FUNC_LOG(1) << " umedia_client_=" << umedia_client_.get();
}

SystemMediaManagerAcb::~SystemMediaManagerAcb() {
  acb_client_->finalize();
}

void SystemMediaManagerAcb::UpdateMediaOption(const Json::Value& media_option) {
  if (media_option.isMember("mediaTransportType"))
    media_transport_type_ = media_option["mediaTransportType"].asString();

  if (media_option.isMember("htmlMediaOption"))
    UpdateHtmlMediaOption(media_option["htmlMediaOption"]);
}

long SystemMediaManagerAcb::Initialize(const bool is_video,
                                       const std::string& app_id,
                                       const ActiveRegionCB& active_region_cb) {
  long player_type = ACB::PlayerType::HTML5_AUDIO;

  is_video_ = is_video;

  if (is_video_) {
    player_type = ACB::PlayerType::VIDEO;
    if (media_transport_type_ == "MIRACAST")
      player_type = ACB::PlayerType::VIDEO_UNMUTED_AFTER_PLAYING;
  }

  player_type = OverridePlayerType(player_type);

  active_region_cb_ = active_region_cb;

  long ret = acb_client_->initialize(player_type, app_id, event_handler_);
  FUNC_LOG(1) << "Initialize ACB player_type=" << player_type
              << " acb_id=" << acb_client_->getAcbId() << " app_id=" << app_id
              << " ret=" << ret;

  DidInitialize();
  return ret;
}

void SystemMediaManagerAcb::AudioMuteChanged(bool mute) {
  DoAudioMute(mute);
}

bool SystemMediaManagerAcb::SetDisplayWindow(const gfx::Rect& out_rect,
                                             const gfx::Rect& in_rect,
                                             bool fullscreen) {
  // There is timing issue that when SetDisplayWindow is called from here and
  // the media is audio file, audio is not played properly sometimes. Checking
  // is_video_ is to prevent this bug.
  if (!is_video_)
    return true;

  if (out_rect.IsEmpty()) {
    LOG(INFO) << __func__ << " - outRect[0, 0, 0, 0], fullscreen is false";
    return acb_client_->setDisplayWindow(0, 0, 0, 0, false,
                                         &task_id_of_set_display_window_);
  } else if (in_rect.IsEmpty()) {
    LOG(INFO) << __func__ << " - outRect: " << out_rect.ToString()
              << ", fullscreen: " << fullscreen;
    return acb_client_->setDisplayWindow(
        out_rect.x(), out_rect.y(), out_rect.width(), out_rect.height(),
        fullscreen, &task_id_of_set_display_window_);
  } else {
    LOG(INFO) << __func__ << " - inRect: " << in_rect.ToString()
              << ", outRect: " << out_rect.ToString()
              << ", fullscreen: " << fullscreen;
    return acb_client_->setCustomDisplayWindow(
        in_rect.x(), in_rect.y(), in_rect.width(), in_rect.height(),
        out_rect.x(), out_rect.y(), out_rect.width(), out_rect.height(),
        fullscreen, &task_id_of_set_display_window_);
  }
  return true;
}

void SystemMediaManagerAcb::SetVisibility(bool visible) {
  if (visibility_ == visible)
    return;
  FUNC_LOG(2) << " call " << (visible ? "stop" : "start") << "Mute";
  visibility_ = visible;
  DoVideoMute(!visible);
}

void SystemMediaManagerAcb::DoVideoMute(bool mute) {
  if (mute)
    acb_client_->startMute(false, true);
  else
    acb_client_->stopMute(false, true);
}

bool SystemMediaManagerAcb::GetVisibility() {
  return visibility_;
}

void SystemMediaManagerAcb::SetAudioFocus() {
  NOTIMPLEMENTED();
}

bool SystemMediaManagerAcb::GetAudioFocus() {
  NOTIMPLEMENTED();
  return false;
}

void SystemMediaManagerAcb::SwitchToAutoLayout() {
  NOTIMPLEMENTED();
}

void SystemMediaManagerAcb::AudioInfoUpdated(
    const struct uMediaServer::audio_info_t& audio_info) {
  Json::Value root;
  Json::Value audio;

  root["context"] = GetMediaId();
  audio["immersive"] = audio_info.immersive;

  root["audio"] = audio;

  WillUpdateAudioInfo(root, audio_info);

  Json::FastWriter writer;
  std::string parameter = writer.write(root);

  DoSetMediaAudioData(parameter);
}

void SystemMediaManagerAcb::VideoInfoUpdated(
    const struct uMediaServer::video_info_t& video_info) {
  Json::Value root;
  Json::Value video;
  root["context"] = GetMediaId();
  root["content"] = "movie";

  if (IsAppName(kLiveDmost))
    root["content"] = "ipch";

  if (IsAppName(kRecordings))
    root["content"] = "atsc30";

  std::string scanType = "video_" + video_info.scanType;
  std::transform(scanType.begin(), scanType.end(), scanType.begin(), ::toupper);
  video["scanType"] = scanType;

  video["width"] = video_info.width;
  video["height"] = video_info.height;
  video["bitRate"] = video_info.bitRate;
  video["frameRate"] = video_info.frameRate;
  video["adaptive"] = IsAdaptiveStreaming();
  video["path"] = is_local_source_ ? "file" : "network";

  gfx::Size par = GetResolutionFromPAR(video_info.pixelAspectRatio);
  video["pixelAspectRatio"]["width"] = par.width();
  video["pixelAspectRatio"]["height"] = par.height();

  struct Media3DInfo media_3dinfo;
  media_3dinfo = GetMedia3DInfo(video_info.mode3D);
  video["data3D"]["currentPattern"] = media_3dinfo.pattern;
  video["data3D"]["typeLR"] = media_3dinfo.type;
  media_3dinfo = GetMedia3DInfo(video_info.actual3D);
  video["data3D"]["originalPattern"] = media_3dinfo.pattern;

  if (video_info.isValidSeiInfo) {
    video["mediaSei"]["displayPrimariesX0"] = video_info.SEI.displayPrimariesX0;
    video["mediaSei"]["displayPrimariesX1"] = video_info.SEI.displayPrimariesX1;
    video["mediaSei"]["displayPrimariesX2"] = video_info.SEI.displayPrimariesX2;
    video["mediaSei"]["displayPrimariesY0"] = video_info.SEI.displayPrimariesY0;
    video["mediaSei"]["displayPrimariesY1"] = video_info.SEI.displayPrimariesY1;
    video["mediaSei"]["displayPrimariesY2"] = video_info.SEI.displayPrimariesY2;
    video["mediaSei"]["whitePointX"] = video_info.SEI.whitePointX;
    video["mediaSei"]["whitePointY"] = video_info.SEI.whitePointY;
    video["mediaSei"]["minDisplayMasteringLuminance"] =
        video_info.SEI.minDisplayMasteringLuminance;
    video["mediaSei"]["maxDisplayMasteringLuminance"] =
        video_info.SEI.maxDisplayMasteringLuminance;
    video["mediaSei"]["maxContentLightLevel"] =
        video_info.SEI.maxContentLightLevel;
    video["mediaSei"]["maxPicAverageLightLevel"] =
        video_info.SEI.maxPicAverageLightLevel;
  }

  if (video_info.isValidVuiInfo) {
    video["mediaVui"]["transferCharacteristics"] =
        video_info.VUI.transferCharacteristics;
    video["mediaVui"]["colorPrimaries"] = video_info.VUI.colorPrimaries;
    video["mediaVui"]["matrixCoeffs"] = video_info.VUI.matrixCoeffs;
    video["mediaVui"]["videoFullRangeFlag"] = video_info.VUI.videoFullRangeFlag;
  }

  video["hdrType"] = video_info.hdrType;

  root["video"] = video;

  WillUpdateVideoInfo(root, video_info);

  Json::FastWriter writer;
  std::string parameter = writer.write(root);

  if (previous_media_video_data_ == parameter)
    return;
  previous_media_video_data_ = parameter;
  FUNC_LOG(2) << " call acb_client_->setMediaVideoData";
  acb_client_->setMediaVideoData(parameter, 0);
}

void SystemMediaManagerAcb::SourceInfoUpdated(bool has_video, bool has_audio) {
  has_video_ = has_video;
  has_audio_ = has_audio;
}

void SystemMediaManagerAcb::PlayStateChanged(PlayState s) {
  next_play_state_ = s;
  UpdateAcbStateIfNeeded();
}

void SystemMediaManagerAcb::AppStateChanged(AppState s) {
  next_app_state_ = s;
  UpdateAcbStateIfNeeded();
}

long SystemMediaManagerAcb::AppStateToAcbAppState(AppState s) {
  switch (s) {
    case AppState::kInit:
      return ACB::AppState::INIT;
    case AppState::kForeground:
      return ACB::AppState::FOREGROUND;
    case AppState::kBackground:
      return ACB::AppState::BACKGROUND;
    default:
      return ACB::AppState::INIT;
  }
  return ACB::AppState::INIT;
}

long SystemMediaManagerAcb::PlayStateToAcbPlayState(PlayState s) {
  switch (s) {
    case PlayState::kUnloaded:
      return ACB::PlayState::UNLOADED;
    case PlayState::kLoaded:
      return ACB::PlayState::LOADED;
    case PlayState::kPlaying:
      return ACB::PlayState::PLAYING;
    case PlayState::kPaused:
      return ACB::PlayState::PAUSED;
    default:
      return ACB::PlayState::UNLOADED;
  }
  return ACB::PlayState::UNLOADED;
}
std::string SystemMediaManagerAcb::GetMediaId() {
  return umedia_client_->getMediaId();
}

void SystemMediaManagerAcb::AcbHandler(long acb_id,
                                       long task_id,
                                       long event_type,
                                       long app_state,
                                       long play_state,
                                       const char* reply) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  FUNC_LOG(1) << " app_state="
              << AppStateToString(static_cast<AppState>(app_state))
              << " play_state="
              << PlayStateToString(static_cast<PlayState>(play_state))
              << " acb_id=" << acb_id << " task_id=" << task_id
              << " eventType=" << event_type
              << " reply=" << (reply != nullptr ? reply : "nullptr");
}

void SystemMediaManagerAcb::UpdateAcbStateIfNeeded() {
  bool app_state_changed = (app_state_ != next_app_state_);
  bool play_state_changed = (play_state_ != next_play_state_);

  // If states are not changed just return;
  if (!app_state_changed && !play_state_changed)
    return;

  if (play_state_changed && next_play_state_ == PlayState::kLoaded) {
    UpdateMediaLoadInfo();
    acb_client_->setMediaId(GetMediaId());
  }

  if (next_app_state_ == AppState::kInit)
    next_app_state_ = AppState::kForeground;

  // Acb don't need kPaused state under kBackground
  if ((app_state_ == AppState::kBackground && !app_state_changed) &&
      next_play_state_ == PlayState::kPaused) {
    LOG(INFO) << __func__ << " ignore PlayState:kPaused under kBackground";
    return;
  }

  // Acb needs kLoaded state when switching BG <-> FG
  if (app_state_changed && next_play_state_ > PlayState::kLoaded)
    next_play_state_ = PlayState::kLoaded;

  WillSetAcbState(next_app_state_, next_play_state_);

  long ret = SetAcbStateInternal(next_app_state_, next_play_state_);

  FUNC_LOG(1) << " app_state=" << AppStateToString(app_state_)
              << " play_state=" << PlayStateToString(play_state_)
              << " acb_id=" << acb_client_->getAcbId()
              << " task_id=" << task_id_of_set_state_ << " ret=" << ret;
  //<< " from " << func << ":" << line;
}

long SystemMediaManagerAcb::SetAcbStateInternal(AppState app, PlayState play) {
  if (app == app_state_ && play == play_state_) {
    FUNC_LOG(1) << " already app_state=" << AppStateToString(app_state_)
                << " play_state=" << PlayStateToString(play_state_);
    return 1;
  }

  long acb_app_state = AppStateToAcbAppState(app);
  long acb_play_state = PlayStateToAcbPlayState(play);
  LOG(INFO) << __func__ << " acb_client_->setState(" << acb_app_state << ", "
            << acb_play_state << ")";
  long ret = acb_client_->setState(acb_app_state, acb_play_state,
                                   &task_id_of_set_state_);

  if (ret > 0) {
    app_state_ = app;
    play_state_ = play;
  } else {
    LOG(ERROR) << __func__ << " tried to set [ app=" << AppStateToString(app)
               << " play=" << PlayStateToString(play)
               << "] failed to set acbState";
  }
  return ret;
}

bool SystemMediaManagerAcb::IsAppName(const char* app_name) {
  return base::StartsWith(app_id_, app_name,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool SystemMediaManagerAcb::IsAdaptiveStreaming() {
  if (media_transport_type_.compare(0, 3, "HLS") == 0 ||
      media_transport_type_ == "MIRACAST" || media_transport_type_ == "MSIIS" ||
      media_transport_type_ == "MPEG-DASH" ||
      media_transport_type_ == "WIDEVINE")
    return true;
  return false;
}

}  // namespace media
