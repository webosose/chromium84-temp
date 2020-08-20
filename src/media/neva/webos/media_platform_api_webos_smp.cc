// Copyright 2018-2020 LG Electronics, Inc.
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

#include "media/neva/webos/media_platform_api_webos_smp.h"

#pragma GCC optimize("rtti")
#include <mediaAPIs.hpp>
#pragma GCC reset_options

#include <queue>
#include <set>
#include <utility>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "content/public/common/content_switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/neva/webos/starfish_media_pipeline_error.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(ENABLE_PLAYREADY_CDM)
#include "playready_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.
#endif

#if defined(ENABLE_LG_SVP)
#include "media/neva/webos/svp_util.h"
#define SVP_VERSION 30
#endif  // defined(ENABLE_LG_SVP)

namespace media {

#define BIND_TO_RENDER_LOOP(function)              \
  (DCHECK(task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::Bind(function, this)))

const size_t kMaxBufferQueueSize = 15 * 1024 * 1024;  // 15MB
const int kMaxFeedAheadSeconds = 5;
const int kMaxFeedAudioVideoDeltaSeconds = 1;

// Lock object for protecting |g_instance_set|.
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

using InstanceSet = std::set<MediaPlatformAPIWebOSSMP*>;

base::LazyInstance<InstanceSet>::Leaky g_instance_set =
    LAZY_INSTANCE_INITIALIZER;

void InstanceCreated(MediaPlatformAPIWebOSSMP* instance) {
  base::AutoLock lock(*g_lock.Pointer());
  g_instance_set.Get().insert(instance);
}

void InstanceDeleted(MediaPlatformAPIWebOSSMP* instance) {
  base::AutoLock lock(*g_lock.Pointer());
  g_instance_set.Get().erase(instance);
}

bool IsAliveInstance(MediaPlatformAPIWebOSSMP* instance) {
  base::AutoLock lock(*g_lock.Pointer());
  return g_instance_set.Get().find(instance) != g_instance_set.Get().end();
}

// static
scoped_refptr<MediaPlatformAPI> MediaPlatformAPI::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    bool video,
    const std::string& app_id,
    const NaturalVideoSizeChangedCB& natural_video_size_changed_cb,
    const base::Closure& resume_done_cb,
    const base::Closure& suspend_done_cb,
    const ActiveRegionCB& active_region_cb,
    const PipelineStatusCB& error_cb) {
  return base::MakeRefCounted<MediaPlatformAPIWebOSSMP>(
      media_task_runner, video, app_id, natural_video_size_changed_cb,
      resume_done_cb, suspend_done_cb, active_region_cb, error_cb);
}

// static
bool MediaPlatformAPI::IsAvailable() {
  return true;
}

MediaPlatformAPIWebOSSMP::MediaPlatformAPIWebOSSMP(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool video,
    const std::string& app_id,
    const NaturalVideoSizeChangedCB& natural_video_size_changed_cb,
    const base::Closure& resume_done_cb,
    const base::Closure& suspend_done_cb,
    const ActiveRegionCB& active_region_cb,
    const PipelineStatusCB& error_cb)
    : task_runner_(task_runner),
      app_id_(app_id),
      natural_video_size_changed_cb_(natural_video_size_changed_cb),
      resume_done_cb_(resume_done_cb),
      suspend_done_cb_(suspend_done_cb),
      active_region_cb_(active_region_cb),
      error_cb_(error_cb),
      weak_factory_(this) {
  starfish_media_apis_.reset(new StarfishMediaAPIs());
  buffer_queue_.reset(new BufferQueue());
#if defined(USE_ACB)
  acb_client_.reset(new Acb);
  if (!acb_client_)
    return;

  EventCallback handler = std::bind(
      &MediaPlatformAPIWebOSSMP::AcbHandler, this, acb_client_->getAcbId(),
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
      std::placeholders::_4, std::placeholders::_5);
  if (!acb_client_->initialize(
          video ? ACB::PlayerType::MSE : ACB::PlayerType::AUDIO, app_id_,
          handler)) {
    return;
  }

// TODO: setVideoRenderMode API was originally made for Quad mode/VTV.
// After those features are enabled, we have to modify it for proper work.
#if defined(USE_SIGNAGE_MEDIA)
  bool is_quad_mode = false;
  bool is_vtv = false;
  acb_client_->setVideoRenderMode(is_quad_mode, is_vtv);
#endif

#if defined(USE_VIDEO_TEXTURE)
  ActiveRegionCallback active_region_handler = std::bind(
      &MediaPlatformAPIWebOSSMP::OnActiveRegion, this, std::placeholders::_1);
  acb_client_->setActiveRegionCallback(active_region_handler);
#endif
#endif  // defined(USE_ACB)

  state_ = CREATED;
  InstanceCreated(this);
}

MediaPlatformAPIWebOSSMP::~MediaPlatformAPIWebOSSMP() {
  key_system_.clear();

  is_destructed_ = true;
  is_finalized_ = true;

#if defined(USE_ACB)
  acb_client_->setState(ACB::AppState::FOREGROUND, ACB::PlayState::UNLOADED);
  acb_client_->finalize();
#endif  // defined(USE_ACB)

  starfish_media_apis_.reset(NULL);

  ResetFeedInfo();

  InstanceDeleted(this);
}

// static
void MediaPlatformAPIWebOSSMP::Callback(const gint type,
                                        const gint64 numValue,
                                        const gchar* strValue,
                                        void* data) {
  MediaPlatformAPIWebOSSMP* target =
      static_cast<MediaPlatformAPIWebOSSMP*>(data);
  if (!target || !IsAliveInstance(target)) {
    return;
  }

  std::string str_value(strValue ? strValue : std::string());
  target->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPlatformAPIWebOSSMP::DispatchCallback,
                 base::Unretained(target), type, numValue, str_value));
}

void MediaPlatformAPIWebOSSMP::Initialize(
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config,
    const PipelineStatusCB& init_cb) {
  DCHECK(!init_cb.is_null());

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPlatformAPIWebOSSMP::Initialize, this,
                              audio_config, video_config, init_cb));
    return;
  }

  audio_config_ = audio_config;
  video_config_ = video_config;
  init_cb_ = init_cb;

#if defined(USE_TV_MEDIA) && defined(USE_ACB)
  if (!video_config_.IsValidConfig())
    acb_client_->changePlayerType(ACB::PlayerType::AUDIO);
#endif  // defined(USE_TV_MEDIA) && defined(USE_ACB)

  std::string payload = MakeLoadPayload(0);
  if (payload.empty())
    return;

  if (is_suspended_) {
    released_media_resource_ = true;
    std::move(init_cb_).Run(PIPELINE_OK);

    return;
  }

  starfish_media_apis_->setExternalContext(g_main_context_default());
  starfish_media_apis_->notifyForeground();

  if (!starfish_media_apis_->Load(payload.c_str(),
                                  &MediaPlatformAPIWebOSSMP::Callback, this)) {
    std::move(error_cb_).Run(PIPELINE_ERROR_DECODE);
    return;
  }

  std::move(init_cb_).Run(PIPELINE_OK);
}

void MediaPlatformAPIWebOSSMP::SetDisplayWindow(const gfx::Rect& rect,
                                                const gfx::Rect& in_rect,
                                                bool fullscreen) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPlatformAPIWebOSSMP::SetDisplayWindow, this,
                              rect, in_rect, fullscreen));
    return;
  }

  window_rect_ = rect;
  window_in_rect_ = in_rect;
  int_fullscreen_ = !fullscreen ? 0 : 1;

#if defined(USE_ACB)
  acb_client_->setCustomDisplayWindow(
      window_in_rect_.x(), window_in_rect_.y(), window_in_rect_.width(),
      window_in_rect_.height(), window_rect_.x(), window_rect_.y(),
      window_rect_.width(), window_rect_.height(), fullscreen);
#endif  // defined(USE_ACB)
}

bool MediaPlatformAPIWebOSSMP::Feed(const scoped_refptr<DecoderBuffer>& buffer,
                                    FeedType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(neva): Remove |FeedInternal| if possible. We need to change usage of
  // this(see HoleframeVideoDecoder).
  return FeedInternal(buffer, type);
}

bool MediaPlatformAPIWebOSSMP::FeedInternal(
    const scoped_refptr<DecoderBuffer>& buffer,
    FeedType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (is_finalized_ || released_media_resource_)
    return true;

  // When MediaPlatformAPIWebOSSMP recovers from finalization, the all
  // information on audio & video are gone. It is required to feed key
  // frames first.
  if (!buffer->is_key_frame() &&
      ((type == FeedType::kAudio && feeded_audio_pts_ == kNoTimestamp) ||
       (type == FeedType::kVideo && feeded_video_pts_ == kNoTimestamp))) {
    return true;
  }

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_LG_SVP)
  // Feed buffer for decoding through Secure Video Path.
  // In this function, various types of buffer can be provided:
  // 1. normal audio buffer
  // 2. widevine key system - unencrypted audio buffer
  // 3. widevine key system - decrypted audio buffer
  // 4. normal video buffer
  // 5. widevine key system - unencrypted video buffer - [A]
  // 6. widevine key system - decrypted video buffer - [B]
  // In above cases, we are interested in only [A] and [B].
  // Also in case of [B], we assume that the buffer is already use svp,
  // so we convert [A] to svp buffer at here.
  // Note: 'unecrypted' means it was never encrypted. 'decrypted' means
  // it is decrypted buffer that decrypted by DecryptingDemuxerStream.
  if (key_system_ == kWidevineKeySystem && !buffer->use_svp_serialized_data() &&
      type == FeedType::kVideo) {
    scoped_refptr<DecoderBuffer> in_buffer = MakeSvpInBandHeader(buffer);
    buffer_queue_->Push(in_buffer, type);
  } else {
    buffer_queue_->Push(buffer, type);
  }
#else
  buffer_queue_->Push(buffer, type);
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_LG_SVP)

  while (!buffer_queue_->Empty()) {
    switch (FeedBuffer(buffer_queue_->Front().first,
                       buffer_queue_->Front().second)) {
      case FeedStatus::kSucceeded:
        buffer_queue_->Pop();
        continue;
      case FeedStatus::kFailed:
        return false;
      case FeedStatus::kOverflowed:
        if (buffer_queue_->DataSize() > kMaxBufferQueueSize) {
          return false;
        }
        return true;
    }
  }
  return true;
}

MediaPlatformAPIWebOSSMP::FeedStatus MediaPlatformAPIWebOSSMP::FeedBuffer(
    const scoped_refptr<DecoderBuffer>& buffer,
    FeedType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (is_finalized_ || released_media_resource_)
    return FeedStatus::kSucceeded;

  uint64_t time = buffer->timestamp().InMicroseconds() * 1000;
  const void* data = buffer->data();
  size_t size = buffer->data_size();

  if (buffer->end_of_stream()) {
    if (type == FeedType::kAudio)
      audio_eos_received_ = true;
    else
      video_eos_received_ = true;
    if (audio_eos_received_ && video_eos_received_)
      PushEOS();
    return FeedStatus::kSucceeded;
  }

  std::stringstream convert_invert;
  convert_invert << std::hex << data;

  Json::Value root;
  Json::FastWriter writer;
  std::string parameter;

  root["bufferAddr"] = convert_invert.str();
  root["bufferSize"] = size;
  root["pts"] = time;
  root["esData"] = static_cast<int>(type);
  parameter = writer.write(root);

  std::string result = starfish_media_apis_->Feed(parameter.c_str());
  std::size_t found = result.find(std::string("Ok"));
  if (found == std::string::npos) {
    found = result.find(std::string("BufferFull"));
    if (found != std::string::npos) {
      return FeedStatus::kOverflowed;
    }
    return FeedStatus::kFailed;
  }

  if (type == FeedType::kAudio)
    feeded_audio_pts_ = buffer->timestamp();
  else
    feeded_video_pts_ = buffer->timestamp();
  return FeedStatus::kSucceeded;
}

void MediaPlatformAPIWebOSSMP::Seek(base::TimeDelta time) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPlatformAPIWebOSSMP::Seek, this, time));
    return;
  }

  if (!starfish_media_apis_)
    return;

  UpdateCurrentTime(time);
  ResetFeedInfo();

  if (!load_completed_) {
    // clear incompletely loaded pipeline
    if (resume_time_ != time) {
      starfish_media_apis_.reset(NULL);
      ReInitialize(time);
    }
    return;
  }

  SetState(SEEKING);

  unsigned seek_time = static_cast<unsigned>(time.InMilliseconds());
  std::string seek_string = std::to_string(seek_time);
  starfish_media_apis_->Seek(seek_string.c_str());
}

void MediaPlatformAPIWebOSSMP::SetPlaybackRate(float playback_rate) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPlatformAPIWebOSSMP::SetPlaybackRate, this,
                              playback_rate));
    return;
  }

  VLOG(1) << " rate(" << playback_rate_ << " -> " << playback_rate << ")";

  if (!starfish_media_apis_)
    return;

  // Pause case
  if (playback_rate == 0.0f) {
    Pause();
    return;
  }

  pending_playback_rate_ = playback_rate;
  Play();
}

std::string MediaPlatformAPIWebOSSMP::GetMediaID() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!starfish_media_apis_)
    return std::string();

  const char* media_id = starfish_media_apis_->getMediaID();
  return std::string(media_id ? media_id : std::string());
}

void MediaPlatformAPIWebOSSMP::Suspend(SuspendReason reason) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlatformAPIWebOSSMP::Suspend, this, reason));
    return;
  }

  if (!starfish_media_apis_) {
    if (!suspend_done_cb_.is_null())
      suspend_done_cb_.Run();
    return;
  }

  is_suspended_ = true;
  if (load_completed_ && is_finalized_)
    starfish_media_apis_->notifyBackground();

#if defined(USE_ACB)
  acb_client_->setState(ACB::AppState::BACKGROUND, ACB::PlayState::LOADED);
#endif  // defined(USE_ACB)

#if defined(USE_TV_MEDIA)
  if (reason == SuspendReason::kSuspendedByPolicy)
    Unload();
#endif

  if (!suspend_done_cb_.is_null())
    suspend_done_cb_.Run();
}

void MediaPlatformAPIWebOSSMP::Resume(
    base::TimeDelta paused_time,
    RestorePlaybackMode restore_playback_mode) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MediaPlatformAPIWebOSSMP::Resume, this,
                                      paused_time, restore_playback_mode));
    return;
  }

  is_suspended_ = false;
  if (released_media_resource_) {
    ReInitialize(paused_time);
    if (!resume_done_cb_.is_null())
      resume_done_cb_.Run();
    return;
  }

  if (load_completed_)
    starfish_media_apis_->notifyForeground();

#if defined(USE_ACB)
  acb_client_->setState(ACB::AppState::FOREGROUND, ACB::PlayState::LOADED);
#endif  // defined(USE_ACB)

  if (!resume_done_cb_.is_null())
    resume_done_cb_.Run();
}

bool MediaPlatformAPIWebOSSMP::AllowedFeedVideo() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!video_config_.IsValidConfig() || !IsFeedableState())
    return false;

  if (feeded_video_pts_ == kNoTimestamp)
    return true;

  base::TimeDelta video_audio_delta = feeded_video_pts_ - feeded_audio_pts_;
  base::TimeDelta buffered_video_time = feeded_video_pts_ - GetCurrentTime();

  return audio_config_.IsValidConfig()
             ? video_audio_delta <
                   base::TimeDelta::FromSeconds(kMaxFeedAudioVideoDeltaSeconds)
             : buffered_video_time <
                   base::TimeDelta::FromSeconds(kMaxFeedAheadSeconds);
}

bool MediaPlatformAPIWebOSSMP::AllowedFeedAudio() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!audio_config_.IsValidConfig() || !IsFeedableState())
    return false;

  if (feeded_audio_pts_ == kNoTimestamp)
    return true;

  base::TimeDelta buffered_audio_time = feeded_audio_pts_ - GetCurrentTime();

  // TODO(neva) webOS media pipeline updates current time by video
  // if video and audio are available.
  // We have to allow audio feed if video is EOS and
  // audio still has data to be able to feed.
  if (video_config_.IsValidConfig() && video_eos_received_)
    return true;

  return buffered_audio_time <
         base::TimeDelta::FromSeconds(kMaxFeedAheadSeconds);
}

void MediaPlatformAPIWebOSSMP::Finalize() {}

void MediaPlatformAPIWebOSSMP::SetKeySystem(const std::string& key_system) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlatformAPIWebOSSMP::SetKeySystem, this, key_system));
    return;
  }

  LOG(INFO) << "Setting key_system: " << key_system;
  key_system_ = key_system;
}

void MediaPlatformAPIWebOSSMP::SetPlaybackVolume(double volume) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlatformAPIWebOSSMP::SetPlaybackVolume, this, volume));
    return;
  }

  playback_volume_ = volume;
  if (!load_completed_)
    return;

  int volume_in_percent = volume * 100;
#if defined(USE_SIGNAGE_MEDIA) && defined(USE_ACB)
  acb_client_->setVolumeMute((volume_in_percent == 0) ? true : false);
#endif

  Json::FastWriter writer;
  Json::Value payload;

  payload["volume"] = volume_in_percent;
  payload["ease"]["duration"] = 0;
  payload["ease"]["type"] = "Linear";

  std::string output = writer.write(payload);
  if (starfish_media_apis_)
    starfish_media_apis_->setVolume(output.c_str());
}

bool MediaPlatformAPIWebOSSMP::IsEOSReceived() {
  std::lock_guard<std::recursive_mutex> lock(recursive_mutex_);
  return received_eos_;
}

#if defined(USE_TV_MEDIA)
void MediaPlatformAPIWebOSSMP::Unload() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (load_completed_) {
    load_completed_ = false;
    released_media_resource_ = true;

#if defined(USE_ACB)
    acb_client_->setState(
        is_suspended_ ? ACB::AppState::BACKGROUND : ACB::AppState::FOREGROUND,
        ACB::PlayState::UNLOADED);
#endif  // defined(USE_ACB)

    if (starfish_media_apis_) {
      is_finalized_ = true;
      starfish_media_apis_.reset(NULL);
    }
  }
}
#endif

#if defined(USE_VIDEO_TEXTURE) && defined(USE_ACB)
void MediaPlatformAPIWebOSSMP::OnActiveRegion(ACB::WindowRect active_region) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPlatformAPIWebOSSMP::OnActiveRegion, this,
                              active_region));
    return;
  }

  if (!active_region_cb_.is_null()) {
    active_region_cb_.Run(gfx::Rect(active_region.left_, active_region.top_,
                                    active_region.width_,
                                    active_region.height_));
  }
}
#endif

void MediaPlatformAPIWebOSSMP::SwitchToAutoLayout() {
#if defined(USE_VIDEO_TEXTURE) && defined(USE_ACB)
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlatformAPIWebOSSMP::SwitchToAutoLayout, this));
    return;
  }

  acb_client_->switchToAutoLayout();
#endif
}

void MediaPlatformAPIWebOSSMP::DispatchCallback(const gint type,
                                                const gint64 numValue,
                                                const std::string& strValue) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (is_finalized_)
    return;

  switch (static_cast<PF_EVENT_T>(type)) {
    case PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED: {
      load_completed_ = true;

      if (player_event_cb_)
        player_event_cb_.Run(PlayerEvent::kLoadCompleted);

#if defined(USE_ACB)
#if defined(USE_TV_MEDIA)
      acb_client_->setSinkType(ACB::StateSinkType::MAIN);
#elif defined(USE_SIGNAGE_MEDIA)
      acb_client_->setVideoSink();
#endif
      acb_client_->setMediaId(GetMediaID());
      acb_client_->setState(
          is_suspended_ ? ACB::AppState::BACKGROUND : ACB::AppState::FOREGROUND,
          ACB::PlayState::LOADED);
#endif  // defined(USE_ACB)

      if (pending_play_)
        Play();
      SetPlaybackVolume(playback_volume_);
      break;
    }
    case PF_EVENT_TYPE_STR_STATE_UPDATE__PAUSED:
      break;
    case PF_EVENT_TYPE_STR_STATE_UPDATE__ENDOFSTREAM:
      SetEOSReceived(true);
      break;
    case PF_EVENT_TYPE_STR_STATE_UPDATE__SEEKDONE:
      if (is_playing_)
        Play();
      else {
        // TODO(neva) : Why?
        SetState(PLAYING);
      }
      if (player_event_cb_)
        player_event_cb_.Run(PlayerEvent::kSeekDone);
      break;
    case PF_EVENT_TYPE_STR_STATE_UPDATE__PLAYING:
      break;
    case PF_EVENT_TYPE_STR_VIDEO_INFO:
      SetMediaVideoData(strValue);
      break;
    case PF_EVENT_TYPE_INT_CURRENT_TIME_PERI:
      if (state_ != SEEKING)
        UpdateCurrentTime(base::TimeDelta::FromNanoseconds(numValue));
      break;
    case PF_EVENT_TYPE_INT_ERROR:
      if (numValue == SMP_RM_RELATED_ERROR) {
        is_finalized_ = true;
        released_media_resource_ = true;
        load_completed_ = false;

        starfish_media_apis_.reset(NULL);

#if defined(USE_ACB)
        acb_client_->setState(ACB::AppState::BACKGROUND,
                              ACB::PlayState::UNLOADED);
#endif  // defined(USE_ACB)
      } else if (SMP_STATUS_IS_100_GENERAL_ERROR(numValue)) {
        if (!error_cb_.is_null())
          std::move(error_cb_).Run(PIPELINE_ERROR_DECODE);
      } else if (SMP_STATUS_IS_200_PLAYBACK_ERROR(numValue)) {
        if (!error_cb_.is_null())
          std::move(error_cb_).Run(PIPELINE_ERROR_DECODE);
      } else if (SMP_STATUS_IS_300_NETWORK_ERROR(numValue)) {
        if (!error_cb_.is_null())
          std::move(error_cb_).Run(PIPELINE_ERROR_NETWORK);
      } else if (numValue == SMP_RESOURCE_ALLOCATION_ERROR) {
        if (!error_cb_.is_null())
          std::move(error_cb_).Run(PIPELINE_ERROR_ABORT);
      }
      break;
    case PF_EVENT_TYPE_INT_BUFFERFULL:
      if (player_event_cb_)
        player_event_cb_.Run(PlayerEvent::kBufferFull);
      break;
    case PF_EVENT_TYPE_INT_BUFFERLOW:
      if (player_event_cb_)
        player_event_cb_.Run(PlayerEvent::kBufferLow);
      break;
    case PF_EVENT_TYPE_INT_DROPPED_FRAME:
      if (!statistics_cb_.is_null()) {
        PipelineStatistics statistics;
        statistics.video_frames_dropped = static_cast<int>(numValue);
        statistics_cb_.Run(statistics);
      }
      dropped_frame_ += numValue;
      VLOG(1) << "DROPPED_FRAME :" << numValue
              << " ,  total : " << dropped_frame_;
      break;
    default:;
  }
}

// private helper functions
void MediaPlatformAPIWebOSSMP::SetState(State next_state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = next_state;
}

void MediaPlatformAPIWebOSSMP::Play() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!load_completed_ || !starfish_media_apis_) {
    pending_play_ = true;
    return;
  }

  if (playback_rate_ != pending_playback_rate_) {
    // change playback rate
    Json::Value root;
    Json::FastWriter writer;
    std::string params;

    root["audioOutput"] = (playback_volume_ == 0) ? false : true;
    root["playRate"] = pending_playback_rate_;
    params = writer.write(root);

    starfish_media_apis_->SetPlayRate(params.c_str());
    playback_rate_ = pending_playback_rate_;
  }

  starfish_media_apis_->Play();
  pending_play_ = false;
  is_playing_ = true;

#if defined(USE_ACB)
  acb_client_->setState(
      is_suspended_ ? ACB::AppState::BACKGROUND : ACB::AppState::FOREGROUND,
      ACB::PlayState::PLAYING);
#endif  // defined(USE_ACB)

  SetState(PLAYING);
}

void MediaPlatformAPIWebOSSMP::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (pending_play_) {
    pending_play_ = false;
    return;
  }

  if (!starfish_media_apis_)
    return;

  starfish_media_apis_->Pause();
  is_playing_ = false;

#if defined(USE_ACB)
  if (!is_suspended_)
    acb_client_->setState(ACB::AppState::FOREGROUND, ACB::PlayState::PAUSED);
#endif  // defined(USE_ACB)
  SetState(PAUSED);
}

void MediaPlatformAPIWebOSSMP::PushEOS() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (is_finalized_ || released_media_resource_)
    return;

  if (starfish_media_apis_)
    starfish_media_apis_->pushEOS();
}

void MediaPlatformAPIWebOSSMP::SetEOSReceived(bool received_eos) {
  std::lock_guard<std::recursive_mutex> lock(recursive_mutex_);
  received_eos_ = received_eos;
}

void MediaPlatformAPIWebOSSMP::ResetFeedInfo() {
  feeded_audio_pts_ = kNoTimestamp;
  feeded_video_pts_ = kNoTimestamp;
  audio_eos_received_ = audio_config_.IsValidConfig() ? false : true;
  video_eos_received_ = video_config_.IsValidConfig() ? false : true;

  SetEOSReceived(false);
  buffer_queue_->Clear();
}

void MediaPlatformAPIWebOSSMP::SetMediaVideoData(
    const std::string& video_info) {
  DCHECK(task_runner_->BelongsToCurrentThread());
#if defined(USE_ACB)
  acb_client_->setMediaVideoData(video_info);
#endif  // defined(USE_ACB)
  Json::Reader reader;
  Json::Value json_video_info;
  if (reader.parse(video_info, json_video_info) && json_video_info.isObject() &&
      json_video_info.isMember("video") &&
      json_video_info["video"].isMember("width") &&
      json_video_info["video"].isMember("height")) {
    gfx::Size natural_video_size(json_video_info["video"]["width"].asUInt(),
                                 json_video_info["video"]["height"].asUInt());
    if (natural_video_size_ != natural_video_size) {
      natural_video_size_ = natural_video_size;
      if (!natural_video_size_changed_cb_.is_null())
        natural_video_size_changed_cb_.Run(natural_video_size_);
    }
  }
}

void MediaPlatformAPIWebOSSMP::SetDisableAudio(bool disable) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlatformAPIWebOSSMP::SetDisableAudio, this, disable));
    return;
  }

  LOG(INFO) << __func__ << " disable=" << disable;
  audio_disabled_ = disable;
}

bool MediaPlatformAPIWebOSSMP::HaveEnoughData() {
  bool has_audio = audio_config_.IsValidConfig();
  bool has_video = video_config_.IsValidConfig();

  base::TimeDelta enough_size = has_video
                                    ? base::TimeDelta::FromMilliseconds(3000)
                                    : base::TimeDelta::FromMilliseconds(500);

  if (state_ == SEEKING || !load_completed_)
    return false;

  base::TimeDelta current_time = GetCurrentTime();

  if (has_audio && !audio_eos_received_ &&
      feeded_audio_pts_ - current_time < enough_size)
    return false;
  if (has_video && !video_eos_received_ &&
      feeded_video_pts_ - current_time < enough_size)
    return false;
  return true;
}

void MediaPlatformAPIWebOSSMP::SetPlayerEventCb(const PlayerEventCB& callback) {
  player_event_cb_ = callback;
}

void MediaPlatformAPIWebOSSMP::SetStatisticsCb(const StatisticsCB& cb) {
  statistics_cb_ = cb;
}

void MediaPlatformAPIWebOSSMP::ReInitialize(base::TimeDelta start_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (is_destructed_)
    return;

  if (is_finalized_)
    is_finalized_ = false;

  uint64_t pts = start_time.InMicroseconds() * 1000;
  resume_time_ = start_time;

  starfish_media_apis_.reset(new StarfishMediaAPIs());

  std::string payload = MakeLoadPayload(pts);
  if (payload.empty())
    return;

  starfish_media_apis_->setExternalContext(g_main_context_default());
  starfish_media_apis_->notifyForeground();

  if (!starfish_media_apis_->Load(payload.c_str(),
                                  &MediaPlatformAPIWebOSSMP::Callback, this)) {
    std::move(error_cb_).Run(PIPELINE_ERROR_DECODE);
    return;
  }

  released_media_resource_ = false;
}

std::string MediaPlatformAPIWebOSSMP::MakeLoadPayload(uint64_t time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  Json::Reader reader;
  Json::FastWriter writer;
  Json::Value payload;
  Json::Value args(Json::arrayValue);
  Json::Value arg;
  Json::Value contents;

  arg["mediaTransportType"] = "BUFFERSTREAM";
  arg["option"]["appId"] = app_id_;
  arg["option"]["adaptiveStreaming"]["maxWidth"] = 1920;
  arg["option"]["adaptiveStreaming"]["maxHeight"] = 1080;
  arg["option"]["adaptiveStreaming"]["maxFrameRate"] = 30;
  arg["option"]["needAudio"] = !audio_disabled_;
#if defined(USE_NEVA_WEBRTC)
  if (video_config_.is_live_stream())
    arg["option"]["transmission"]["contentsType"] = "WEBRTC";
#endif

  if (video_config_.IsValidConfig()) {
#if defined(USE_GAV)
    if (get_media_layer_id().empty()) {
      std::move(error_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
      return std::string();
    }
    arg["option"]["windowId"] = get_media_layer_id();
#endif

    std::string codec_type =
        base::ToUpperASCII(GetCodecName(video_config_.codec()));
    contents["codec"]["video"] = codec_type;

    base::Optional<MediaCodecCapability> capability =
        GetMediaCodecCapabilityForCodec(codec_type);
    if (capability.has_value()) {
      arg["option"]["adaptiveStreaming"]["maxWidth"] = capability->width;
      arg["option"]["adaptiveStreaming"]["maxHeight"] = capability->height;
      arg["option"]["adaptiveStreaming"]["maxFrameRate"] =
          capability->frame_rate;
    } else {
      LOG(INFO) << "[" << this << "] " << __func__
                << " Not found video codec capability for " << codec_type
                << ". Use default capability.";
    }
  }

  if (audio_config_.IsValidConfig()) {
    switch (audio_config_.codec()) {
      case media::kCodecAAC:
        contents["codec"]["audio"] = "AAC";
        break;
      case media::kCodecVorbis:
        contents["codec"]["audio"] = "Vorbis";
        break;
      case media::kCodecAC3:
        contents["codec"]["audio"] = "AC3";
        break;
      case media::kCodecEAC3:
        contents["codec"]["audio"] = "AC3 PLUS";
        break;
      default:
        std::move(error_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
        return std::string();
    }
  }

  contents["esInfo"]["ptsToDecode"] = time;
  contents["esInfo"]["seperatedPTS"] = true;
  contents["format"] = "RAW";
  contents["provider"] = "Chrome";

  bool use_secure_buffer = false;
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system_ == kWidevineKeySystem) {
    use_secure_buffer = true;
    arg["option"]["drm"]["type"] = "WIDEVINE_MODULAR";
  }
#endif
#if defined(ENABLE_PLAYREADY_CDM)
  if (key_system_ == kPlayReadyMicrosoftKeySystem ||
      key_system_ == kPlayReadyYouTubeKeySystem) {
    use_secure_buffer = true;
    arg["option"]["drm"]["type"] = "PLAYREADY";
  }
#endif
#if defined(ENABLE_LG_SVP)
  if (use_secure_buffer)
    arg["option"]["externalStreamingInfo"]["svpVersion"] = SVP_VERSION;
#endif

  arg["option"]["externalStreamingInfo"]["contents"] = contents;

  args.append(arg);

  payload["args"] = args;

#ifndef NDEBUG
  std::istringstream iss(payload.toStyledString());
  LOG(INFO) << __func__ << " Payload {";
  for (std::string line; std::getline(iss, line);)
    LOG(INFO) << line;
  LOG(INFO) << __func__ << " } Payload";
#endif

  return writer.write(payload);
}

void MediaPlatformAPIWebOSSMP::AcbHandler(long acb_id,
                                          long task_id,
                                          long event_type,
                                          long app_state,
                                          long play_state,
                                          const char* reply) {}

void MediaPlatformAPIWebOSSMP::SetVisibility(bool visible) {
#if defined(USE_ACB)
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPlatformAPIWebOSSMP::SetVisibility, this, visible));
    return;
  }

  VLOG(1) << __func__ << " visible=" << visible;
  if (visible)
    acb_client_->stopMute(false, true);
  else
    acb_client_->startMute(false, true);
#endif  // defined(USE_ACB)
}

bool MediaPlatformAPIWebOSSMP::IsFeedableState() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  switch (state_) {
    case INVALID:
    case FINALIZED:
      return false;
    default:
      return true;
  }
}

void MediaPlatformAPIWebOSSMP::UpdateCurrentTime(const base::TimeDelta& time) {
  if (update_current_time_cb_)
    update_current_time_cb_.Run(time);
  MediaPlatformAPI::UpdateCurrentTime(std::move(time));
}

void MediaPlatformAPIWebOSSMP::SetUpdateCurrentTimeCb(
    const UpdateCurrentTimeCB& callback) {
  update_current_time_cb_ = callback;
}

}  // namespace media
