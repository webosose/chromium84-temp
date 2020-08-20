// Copyright 2016-2018 LG Electronics, Inc.
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

#include "ui/gl/neva/video_texture_manager.h"

#include <sys/types.h>
#include <unistd.h>
#include <vt/vt_openapi.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

#define INFO_LOG(format, ...)

#define DEBUG_LOG(format, ...)

namespace gfx {

class PlatformVideoTextureLib {
 public:
  PlatformVideoTextureLib() {}
  virtual ~PlatformVideoTextureLib() {}

  virtual bool Initialize() { return false; }
  virtual void Terminate() {}
  virtual bool Update(unsigned prev_texture_id,
                      unsigned* texture_id,
                      int* texture_width,
                      int* texture_height) {
    return false;
  }
};

class VTG : public PlatformVideoTextureLib {
 public:
  VTG()
      : resource_id_(0),
        context_id_(0),
        current_texture_id_(0),
        current_texture_width_(0),
        current_texture_height_(0),
        new_texture_available_(true) {}
  virtual ~VTG() {}

  static bool IsSupported();
  bool Initialize() override;
  void Terminate() override;

  bool Update(unsigned prev_texture_id,
              unsigned* texture_id,
              int* texture_width,
              int* texture_height) override;

  void NotifyNewTextureAvailable() {
    new_texture_available_ = true;
    VideoTextureManager::GetInstance()->DispatchCallBacks();
  }

 private:

  static void OnVTEvent(VT_EVENT_TYPE_T type, void* data, void* user_data);

  VT_RESOURCE_ID resource_id_;
  VT_CONTEXT_ID context_id_;
  unsigned current_texture_id_;
  int current_texture_width_;
  int current_texture_height_;
  bool new_texture_available_;
};

// static
bool VTG::IsSupported() {
  unsigned supported = 0;
  if ((VT_IsSystemSupported(&supported) == VT_OK) && supported)
    return true;
  return false;
}

bool VTG::Initialize() {
  unsigned int max_texture_buffer_size = 0;
  VT_RESOLUTION_T max_resolution = {0, 0};
  VT_VIDEO_PATH_TYPE_T path = VT_VIDEO_PATH_MAIN;
  VT_VIDEO_WINDOW_ID window_id;

  if(VT_GetMaxTextureResolution(&max_resolution) != VT_OK)
    goto err;

  if(VT_GetMaxTextureBufferSize(&max_texture_buffer_size) != VT_OK)
    goto err;

  window_id = VT_CreateVideoWindow(path);
  if (window_id == -1)
    goto err;

  if (VT_AcquireVideoWindowResource(window_id, &resource_id_) != VT_OK)
    goto err;

  context_id_ = VT_CreateContext(resource_id_, max_texture_buffer_size);

  // VT_CONTEXT_ID(the type of context_id_) is unsigned,
  // so comparison with -1(signed int) is not correct. (Beomjin Kim)
  /*
  if (context_id_ == -1) {
    goto err_resource;
  }
  */

  if (VT_SetTextureSourceRegion(context_id_, VT_SOURCE_REGION_MAX) != VT_OK)
    goto err_context;

  if (VT_SetTextureSourceLocation(context_id_, VT_SOURCE_LOCATION_DISPLAY) != VT_OK)
    goto err_context;

  VT_RESOLUTION_T vt_resolution;
  vt_resolution.w = 1920;
  vt_resolution.h = 1080;

  if (VT_SetTextureResolution(context_id_, &vt_resolution) != VT_OK)
    goto err_context;

  if (VT_RegisterEventHandler(context_id_, &OnVTEvent, this) != VT_OK) {
    goto err_context;
  }
  return true;

err_context:
  VT_DeleteContext(context_id_);
/*
err_resource:
  VT_ReleaseVideoWindowResource(resource_id_);
*/
err:
  return false;
}

void VTG::Terminate() {
  VT_UnRegisterEventHandler(context_id_);
  VT_DeleteContext(context_id_);
  VT_ReleaseVideoWindowResource(resource_id_);
  current_texture_id_ = 0;
  new_texture_available_ = true;
}

bool VTG::Update(unsigned prev_texture_id,
                 unsigned* texture_id,
                 int* texture_width,
                 int* texture_height) {
  if (!new_texture_available_) {
    *texture_id = current_texture_id_;
    *texture_width = current_texture_width_;
    *texture_height = current_texture_height_;
    return true;
  }

  VT_OUTPUT_INFO_T output_info;
  unsigned int video_texture = 0;

  VT_STATUS_T status = VT_GetAvailableTexture(resource_id_, context_id_,
                                              &video_texture, &output_info);
  if (status != VT_OK) {
    return false;
  }

  if (!output_info.activeRegion.w || !output_info.activeRegion.h ||
      !output_info.maxRegion.w || !output_info.maxRegion.h) {
    return false;
  }

  *texture_id = current_texture_id_ = video_texture;
  *texture_width = current_texture_width_ = output_info.maxRegion.w;
  *texture_height = current_texture_height_ = output_info.maxRegion.h;
  new_texture_available_ = false;
  return true;
}

// static
void VTG::OnVTEvent(VT_EVENT_TYPE_T type,
                                  void* data,
                                  void* user_data) {
  VTG* vtg = reinterpret_cast<VTG*>(user_data);
  switch (type) {
    case VT_AVAILABLE:
      vtg->NotifyNewTextureAvailable();
      break;
    case VT_UNAVAILABLE:
      break;
    case VT_RESOURCE_BUSY:
      break;
    default:
      break;
  }
}

// static
VideoTextureManager* VideoTextureManager::GetInstance() {
  return base::Singleton<VideoTextureManager>::get();
}

VideoTextureManager::VideoTextureManager() : initialized_(false) {}

VideoTextureManager::~VideoTextureManager() {
  Terminate();
}

bool VideoTextureManager::Initialize() {
  if (initialized_)
    return true;

  platform_lib_ = base::WrapUnique(new VTG());

  if (!platform_lib_->Initialize())
    return false;

  initialized_ = true;
  return true;
}

void VideoTextureManager::Terminate() {
  if (!initialized_)
    return;

  platform_lib_->Terminate();
  platform_lib_.reset();

  initialized_ = false;
}

void VideoTextureManager::Register(uint32_t client_id) {
  if (!Initialize()) {
    INFO_LOG("Failed to initialize");
    return;
  }

  if (!client_id)
    return;

  base::AutoLock auto_lock(callback_map_lock_);
  if (callback_map_.find(client_id) == callback_map_.end()) {
    callback_map_[client_id] = base::Closure();
    INFO_LOG("Register a video texture, client_id=%d", client_id);
  }
}

void VideoTextureManager::Unregister(uint32_t client_id) {
  if (!client_id)
    return;

  bool shall_terminate = false;
  {
    base::AutoLock auto_lock(callback_map_lock_);
    if (callback_map_.find(client_id) != callback_map_.end()) {
      callback_map_.erase(client_id);

      INFO_LOG("Unregister a video texture, client_id=%d", client_id);

      if (callback_map_.empty())
        shall_terminate = true;
    }
  }
  if (shall_terminate)
    Terminate();
}

void VideoTextureManager::Update(unsigned prev_texture_id,
                                 unsigned* texture_id,
                                 int* texture_width,
                                 int* texture_height) {
  platform_lib_->Update(prev_texture_id, texture_id, texture_width,
                        texture_height);
}

void VideoTextureManager::SetFrameAvailableCallback(
    uint32_t client_id,
    const base::Closure& callback) {
  if (!client_id)
    return;

  base::AutoLock auto_lock(callback_map_lock_);
  if (callback_map_.find(client_id) != callback_map_.end())
    callback_map_[client_id] = callback;
}

void VideoTextureManager::DispatchCallBacks() {
  base::AutoLock auto_lock(callback_map_lock_);
  for (auto it : callback_map_) {
    base::Closure callback = it.second;
    if (!callback.is_null())
      callback.Run();
  }
}

// static
bool VideoTextureManager::IsVideoTextureSupported() {
  return VTG::IsSupported();
}

}  // namespace gfx
