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

#ifndef MEDIA_NEVA_WEBOS_MEDIA_PLAYER_EXTINPUT_H_
#define MEDIA_NEVA_WEBOS_MEDIA_PLAYER_EXTINPUT_H_

#if defined(USE_ACB)
#include <Acb.h>
#endif

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/neva/webos/luna_service_client.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/neva/media_player_neva_interface.h"

class GURL;

namespace media {

static const long INVALID_ACB_ID = -1;

class MediaPlayerExtInput : public media::MediaPlayerNeva,
                            public base::SupportsWeakPtr<MediaPlayerExtInput> {
 public:
  MediaPlayerExtInput(
      MediaPlayerNevaClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const std::string& app_id);
  virtual ~MediaPlayerExtInput();

  // media::MediaPlayerNeva implementation
  void Initialize(const bool is_video,
                  const double current_time,
                  const std::string& url,
                  const std::string& mime,
                  const std::string& referrer,
                  const std::string& user_agent,
                  const std::string& cookies,
                  const std::string& media_option,
                  const std::string& custom_option) override;

  // Starts the player.
  void Start() override;
  // Pauses the player.
  void Pause() override;
  // Performs seek on the player.
  void Seek(const base::TimeDelta& time) override {}
  // Sets the player volume.
  void SetVolume(double volume) override;
  // Sets the poster image.
  void SetPoster(const GURL& poster) override {}

  void SetRate(double rate) override;
  void SetPreload(Preload preload) override {}
  bool IsPreloadable(const std::string& content_media_option) override {
    return false;
  }
  bool HasVideo() override { return has_video_; }
  bool HasAudio() override { return has_audio_; }
  bool SelectTrack(const MediaTrackType type, const std::string& id) override {
    return false;
  }
  void SwitchToAutoLayout() override {}
  void SetDisplayWindow(const gfx::Rect&,
                        const gfx::Rect&,
                        bool fullScreen,
                        bool forced = false) override;
  bool UsesIntrinsicSize() const override;
  std::string MediaId() const override { return external_input_id_; }
  bool HasAudioFocus() const override { return false; }
  void SetAudioFocus(bool focus) override {}
  bool HasVisibility() const override { return true; }
  void SetVisibility(bool) override {}
  void Suspend(SuspendReason reason) override {}
  void Resume() override {}
  bool RequireMediaResource() const override { return false; }
  bool IsRecoverableOnResume() const override { return true; }
  void SetDisableAudio(bool) override;
  void SetMediaLayerId(const std::string& media_layer_id) override;

  // end of media::MediaPlayerNeva implementation
  // void Dispose() override;

 private:
  friend class base::RefCountedThreadSafe<MediaPlayerExtInput>;

  MediaPlayerNevaClient* client_;

  bool loaded_ = false;
  bool has_video_ = false;
  bool has_audio_ = false;
  float playback_rate_ = 1.0f;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::string app_id_;
  base::LunaServiceClient luna_service_client_{
      base::LunaServiceClient::PrivateBus};
  LSMessageToken subscribe_key_ = LSMESSAGE_TOKEN_INVALID;
  std::string media_layer_id_;
#if defined(USE_ACB)
  std::unique_ptr<Acb> acb_client_{new Acb()};
  long acb_load_task_id_ = 0;
  long acb_id_;
#endif  // defined(USE_ACB)
  GURL url_;
  std::string mime_type_;
  std::string external_input_id_;
  gfx::Rect in_rect_;
  gfx::Rect out_rect_;
  gfx::Size source_size_{1920, 1080};
  bool full_screen_ = false;
  base::Lock lock_;
  bool audio_disabled_ = false;

  void CallswitchExternalInput();
  void PlayPipeline();
  void PausePipeline();
  void ClosePipeline();
  void GetCurrentVideo();
  void HandleGetCurrentVideoReply(const std::string& payload);
  void AcbLoadCompleted();
  void DispatchAcbHandler(long acb_id,
                          long task_id,
                          long event_type,
                          long app_state,
                          long play_state,
                          const char* reply);

  void HandleOpenReply(const std::string& payload);
  void HandleSwitchExternalInputReply(const std::string& payload);

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerExtInput);
};

}  // namespace media

#endif  // MEDIA_NEVA_WEBOS_MEDIA_PLAYER_EXTINPUT_H_
