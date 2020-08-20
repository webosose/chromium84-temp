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

#ifndef MEDIA_NEVA_WEBOS_MEDIA_PLATFORM_API_WEBOS_SMP_H_
#define MEDIA_NEVA_WEBOS_MEDIA_PLATFORM_API_WEBOS_SMP_H_

#if defined(USE_ACB)
#include <Acb.h>
#endif
#include <glib.h>

#include <mutex>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/neva/media_platform_api.h"
#include "ui/gfx/geometry/rect.h"

#if defined(USE_ACB)
class Acb;
#endif
class StarfishMediaAPIs;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MEDIA_EXPORT MediaPlatformAPIWebOSSMP : public MediaPlatformAPI {
 public:
  MediaPlatformAPIWebOSSMP(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      bool video,
      const std::string& app_id,
      const NaturalVideoSizeChangedCB& natural_video_size_changed_cb,
      const base::Closure& resume_done_cb,
      const base::Closure& suspend_done_cb,
      const ActiveRegionCB& active_region_cb,
      const PipelineStatusCB& error_cb);
  ~MediaPlatformAPIWebOSSMP() override;
  static void Callback(const gint type,
                       const gint64 numValue,
                       const gchar* strValue,
                       void* data);

  // MediaPlaformAPIWebOS
  void Initialize(const AudioDecoderConfig& audio_config,
                  const VideoDecoderConfig& video_config,
                  const PipelineStatusCB& init_cb) override;
  void SetDisplayWindow(const gfx::Rect& rect,
                        const gfx::Rect& in_rect,
                        bool fullscreen) override;
  bool Feed(const scoped_refptr<DecoderBuffer>& buffer, FeedType type) override;
  void Seek(base::TimeDelta time) override;
  void Suspend(SuspendReason reason) override;
  void Resume(base::TimeDelta paused_time,
              RestorePlaybackMode restore_playback_mode) override;
  void SetPlaybackRate(float playback_rate) override;
  void SetPlaybackVolume(double volume) override;
  bool AllowedFeedVideo() override;
  bool AllowedFeedAudio() override;
  void Finalize() override;
  void SetKeySystem(const std::string& key_system) override;
  bool IsEOSReceived() override;

  void SwitchToAutoLayout() override;

  void SetVisibility(bool visible) override;
  void SetDisableAudio(bool disable) override;
  bool HaveEnoughData() override;
  void SetPlayerEventCb(const PlayerEventCB& callback) override;
  void SetStatisticsCb(const StatisticsCB& cb) override;

  void UpdateCurrentTime(const base::TimeDelta& time) override;
  void SetUpdateCurrentTimeCb(const UpdateCurrentTimeCB& cb) override;

 private:
  enum State { INVALID = -1, CREATED = 0, PLAYING, PAUSED, SEEKING, FINALIZED };

  enum RestoreDisplayWindowMode {
    DONT_RESTORE_DISPLAY_WINDOW = 0,
    RESTORE_DISPLAY_WINDOW
  };

  enum class FeedStatus { kSucceeded, kOverflowed, kFailed };

  std::string GetMediaID();
  bool FeedInternal(const scoped_refptr<DecoderBuffer>& buffer, FeedType type);
  FeedStatus FeedBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                        FeedType type);
#if defined(USE_TV_MEDIA)
  void Unload();
#endif
#if defined(USE_VIDEO_TEXTURE)
  void OnActiveRegion(ACB::WindowRect active_region);
#endif

  void DispatchCallback(const gint type,
                        const gint64 numValue,
                        const std::string& strValue);

  void SetState(State);
  void Play();
  void Pause();

  void ResetFeedInfo();
  void PushEOS();
  void SetEOSReceived(bool received_eos);
  void SetMediaVideoData(const std::string& video_info);

  void ReInitialize(base::TimeDelta start_time);

  std::string MakeLoadPayload(uint64_t time);
  bool IsFeedableState() const;
  void AcbHandler(long acb_id,
                  long task_id,
                  long event_type,
                  long app_state,
                  long play_state,
                  const char* reply);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::string app_id_;

  gfx::Size natural_video_size_;
  NaturalVideoSizeChangedCB natural_video_size_changed_cb_;
  base::Closure resume_done_cb_;
  base::Closure suspend_done_cb_;
  ActiveRegionCB active_region_cb_;
#if defined(USE_TV_MEDIA)
  base::Closure player_loaded_cb_;
#endif

  PipelineStatusCB error_cb_;
  PipelineStatusCB init_cb_;
  PipelineStatusCB seek_cb_;

  UpdateCurrentTimeCB update_current_time_cb_;

  // For protecting received_eos_ variable.
  std::recursive_mutex recursive_mutex_;

  State state_ = INVALID;

  base::TimeDelta feeded_audio_pts_ = kNoTimestamp;
  base::TimeDelta feeded_video_pts_ = kNoTimestamp;
  bool audio_eos_received_ = false;
  bool video_eos_received_ = false;
  double playback_volume_ = 1.0;
  bool received_eos_ = false;
  bool released_media_resource_ = false;
  bool is_destructed_ = false;
  bool is_suspended_ = false;
  bool load_completed_ = false;
  bool is_finalized_ = false;

  std::unique_ptr<BufferQueue> buffer_queue_;

  base::TimeDelta resume_time_ = kNoTimestamp;

  gfx::Rect window_rect_;
  gfx::Rect window_in_rect_;
  int int_fullscreen_ = 0;

  // Blink uses SetPlaybackRate for Play/Pause and changing rate as well.
  // W'd like to use separate playing_ and playback_rate_ to keep
  // the previous running playback_rate when SetPlaybackRate(0) is called.
  // SetPlaybackRate(0) would change is_playing_ to false but playback_rate_
  // would not be changed. We want to distinguish between play/pause
  // and changing playbackRate
  bool is_playing_ = false;
  bool pending_play_ = false;
  float playback_rate_ = 1.0f;
  float pending_playback_rate_ = playback_rate_;

  std::string media_id_;

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

#if defined(USE_ACB)
  std::unique_ptr<Acb> acb_client_;
#endif
  std::unique_ptr<StarfishMediaAPIs> starfish_media_apis_;

  std::string key_system_;
  bool audio_disabled_ = false;
  PlayerEventCB player_event_cb_;
  StatisticsCB statistics_cb_;
  uint64_t dropped_frame_ = 0;

  base::WeakPtrFactory<MediaPlatformAPIWebOSSMP> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlatformAPIWebOSSMP);
};

}  // namespace media

#endif  // MEDIA_NEVA_WEBOS_MEDIA_PLATFORM_API_WEBOS_SMP_H_
