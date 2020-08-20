// Copyright 2018-2020 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_ACB_H_
#define MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_ACB_H_

#include <Acb.h>
#include <string>
#include "base/memory/weak_ptr.h"
#include "media/neva/webos/system_media_manager.h"

namespace gfx {
class Rect;
}

namespace Json {
class Value;
}

namespace uMediaServer {
struct audio_info_t;
struct video_info_t;
}  // namespace uMediaServer

namespace media {
class UMediaClientImpl;

class SystemMediaManagerAcb : public SystemMediaManager {
 public:
  SystemMediaManagerAcb(
      const base::WeakPtr<UMediaClientImpl>& umedia_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  ~SystemMediaManagerAcb() override;

  long Initialize(const bool is_video,
                  const std::string& app_id,
                  const ActiveRegionCB& active_region_cb) override;
  void UpdateMediaOption(const Json::Value& media_option) override;
  bool SetDisplayWindow(const gfx::Rect& out_rect,
                        const gfx::Rect& in_rect,
                        bool fullscreen) override;
  void SetVisibility(bool visible) override;
  bool GetVisibility() override;
  void SetAudioFocus() override;
  bool GetAudioFocus() override;
  void SwitchToAutoLayout() override;
  void AudioInfoUpdated(
      const struct uMediaServer::audio_info_t& audio_info) override;
  void VideoInfoUpdated(
      const struct uMediaServer::video_info_t& videoInfo) override;
  void SourceInfoUpdated(bool has_video, bool has_audio) override;
  void AppStateChanged(AppState s) override;
  void PlayStateChanged(PlayState s) override;
  void AudioMuteChanged(bool mute) override;
  bool SendCustomMessage(const std::string& message) override { return true; }
  void EofReceived() override {}

 protected:
  long AppStateToAcbAppState(AppState s);
  long PlayStateToAcbPlayState(PlayState s);
  long SetAcbStateInternal(AppState app, PlayState play);
  std::string GetMediaId();
  AppState CurrentAppState() { return app_state_; }
  PlayState CurrentPlayState() { return play_state_; }

  std::unique_ptr<Acb> acb_client_{new Acb()};
  long acb_id_;
  std::string app_id_;
  std::string media_id_;
  std::string media_transport_type_;
  std::string previous_media_video_data_;
  bool visibility_ = true;
  bool is_local_source_ = false;
  long task_id_of_set_display_window_ = 0;
  long task_id_of_set_state_ = 0;
  ActiveRegionCB active_region_cb_;
  bool has_audio_ = false;
  bool has_video_ = false;

 private:
  struct MediaOption {
    std::string video_sink_type_;
    std::string video_sink_purpose_;
    std::string dass_action_;
    bool use_dass_control_;
  };

  // Templete methods to customise for each product

  // Before setting acb state |WillSetAcbState| will be called.
  // From this appstate or playstate can be modified
  virtual void WillSetAcbState(AppState& app, PlayState& play) {}
  // |WillUpdateVideoInfo| provides a chance to add custom value
  virtual void WillUpdateVideoInfo(
      Json::Value& root,
      const uMediaServer::video_info_t& video_info) {}
  // |WillUpdateAudioInfo| provides a chance to add custom value
  virtual void WillUpdateAudioInfo(
      Json::Value& root,
      const uMediaServer::audio_info_t& audio_info) {}
  // |UpdateHtmlMediaOption| provides processing custom html media option
  virtual void UpdateHtmlMediaOption(const Json::Value& html_media_option) {}
  // |UpdateMediaLoadInfo| is called when PlayState is set kLoaded
  virtual void UpdateMediaLoadInfo() {}
  // OverridePlayerType based on its custom type
  virtual long OverridePlayerType(long player_type) { return player_type; }
  // Perform AudioMute
  virtual void DoAudioMute(bool mute) {}
  // Perform VideoMute
  virtual void DoVideoMute(bool mute);
  // Perform setMediaAudioData
  virtual void DoSetMediaAudioData(const std::string& param) {}
  // |DidInitialize| is called right after Initialized.
  virtual void DidInitialize() {}

  void AcbHandler(long acb_id,
                  long task_id,
                  long event_type,
                  long app_state,
                  long play_state,
                  const char* reply);
  void UpdateAcbStateIfNeeded();
  bool IsAdaptiveStreaming();
  bool IsAppName(const char* app_name);

  PlayState play_state_ = PlayState::kUnloaded;
  PlayState next_play_state_ = PlayState::kUnloaded;
  AppState app_state_ = AppState::kInit;
  AppState next_app_state_ = AppState::kInit;
  bool is_video_ = false;

  struct MediaOption media_option_;
  EventCallback event_handler_;
  base::WeakPtr<UMediaClientImpl> umedia_client_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<SystemMediaManagerAcb> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_NEVA_WEBOS_SYSTEM_MEDIA_MANAGER_ACB_H_
