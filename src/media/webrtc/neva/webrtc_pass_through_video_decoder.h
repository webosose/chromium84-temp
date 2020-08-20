// Copyright 2020 LG Electronics, Inc.
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

#ifndef MEDIA_WEBRTC_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_
#define MEDIA_WEBRTC_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "ui/gfx/geometry/size.h"

namespace webrtc {
class EncodedImage;
class SdpVideoFormat;
class VideoCodec;
}  // namespace webrtc

namespace media {

class MEDIA_EXPORT WebRtcPassThroughVideoDecoder : public webrtc::VideoDecoder {
 public:
  class Client {
   public:
    virtual bool HasAvailableResources() = 0;
  };

  // Creates and initializes an WebRtcPassThroughVideoDecoder.
  static std::unique_ptr<WebRtcPassThroughVideoDecoder> Create(
      const webrtc::SdpVideoFormat& format);

  static WebRtcPassThroughVideoDecoder* FromId(uint32_t decoder_id);

  virtual ~WebRtcPassThroughVideoDecoder();

  // Implements webrtc::VideoDecoder
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  int32_t Release() override;
  const char* ImplementationName() const override;

  // Request key frame.
  void RequestKeyFrame();

  // Set decoder client instance
  void SetClient(Client* client);

 private:
  // Called on the worker thread.
  WebRtcPassThroughVideoDecoder(media::VideoCodec video_codec);

  int32_t ReturnEncodedFrame(const webrtc::EncodedImage& input_image);

  // Construction parameters.
  media::VideoCodec video_codec_;
  VideoPixelFormat video_pixel_format_;

  // Not owned. Should not be deleted.
  Client* client_ = nullptr;

  gfx::Size frame_size_;

  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;

  bool initialized_ = false;
  bool key_frame_required_ = true;

  bool media_decoder_acquired_ = false;
  bool media_decoder_available_ = true;

  int propagation_cnt_ = -1;
  base::TimeTicks start_timestamp_;

  uint32_t decoder_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WebRtcPassThroughVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_
