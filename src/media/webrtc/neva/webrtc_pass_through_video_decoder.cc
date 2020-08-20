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

#include "media/webrtc/neva/webrtc_pass_through_video_decoder.h"

#include <mutex>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "media/base/video_frame.h"
#include "media/neva/media_preferences.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"
#include "third_party/webrtc/rtc_base/helpers.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace media {

namespace {

const char* kImplementationName = "WebRtcPassThroughVideoDecoder";

// Error propogation threshold value
constexpr int kVp8ErrorPropagationTh = 30;

// Map webrtc::VideoCodecType to media::VideoCodec.
media::VideoCodec ToVideoCodec(webrtc::VideoCodecType webrtc_codec) {
  switch (webrtc_codec) {
    case webrtc::kVideoCodecVP8:
      return media::kCodecVP8;
    case webrtc::kVideoCodecVP9:
      return media::kCodecVP9;
    case webrtc::kVideoCodecH264:
      return media::kCodecH264;
    default:
      break;
  }
  return media::kUnknownVideoCodec;
}
}  // namespace

typedef std::map<uint32_t, WebRtcPassThroughVideoDecoder*> WebRtcDecoderMap;
WebRtcDecoderMap webrtc_video_decoder_map_;
std::mutex webrtc_video_decoder_lock_;

// static
std::unique_ptr<WebRtcPassThroughVideoDecoder>
WebRtcPassThroughVideoDecoder::Create(
    const webrtc::SdpVideoFormat& sdp_format) {
  DVLOG(1) << __func__ << "(" << sdp_format.name << ")";

  const webrtc::VideoCodecType webrtc_codec_type =
      webrtc::PayloadStringToCodecType(sdp_format.name);

  // Bail early for unknown codecs.
  media::VideoCodec video_codec = ToVideoCodec(webrtc_codec_type);
  if (video_codec == media::kUnknownVideoCodec)
    return nullptr;

  // Fallback to software decoder if not supported by platform.
  const std::string& codec_name = base::ToUpperASCII(GetCodecName(video_codec));
  const auto capability =
      MediaPreferences::Get()->GetMediaCodecCapabilityForCodec(codec_name);
  if (!capability.has_value()) {
    VLOG(1) << codec_name << " is unsupported by HW decoder";
    return nullptr;
  }

  return base::WrapUnique(new WebRtcPassThroughVideoDecoder(video_codec));
}

// static
WebRtcPassThroughVideoDecoder* WebRtcPassThroughVideoDecoder::FromId(
    uint32_t decoder_id) {
  VLOG(1) << __func__ << " Decoder requested for id: " << decoder_id;

  std::lock_guard<std::mutex> lock(webrtc_video_decoder_lock_);
  WebRtcDecoderMap::iterator it = webrtc_video_decoder_map_.find(decoder_id);
  if (it == webrtc_video_decoder_map_.end()) {
    LOG(ERROR) << __func__ << " Decoder not found for id: " << decoder_id;
    return nullptr;
  }
  return it->second;
}

WebRtcPassThroughVideoDecoder::WebRtcPassThroughVideoDecoder(
    media::VideoCodec video_codec)
    : video_codec_(video_codec), decoder_id_(rtc::CreateRandomNonZeroId()) {
  {
    std::lock_guard<std::mutex> lock(webrtc_video_decoder_lock_);
    webrtc_video_decoder_map_[decoder_id_] = this;
  }

  VLOG(1) << __func__ << "[" << this << "] "
          << " codec: " << GetCodecName(video_codec) << " id: " << decoder_id_;
}

WebRtcPassThroughVideoDecoder::~WebRtcPassThroughVideoDecoder() {
  VLOG(1) << __func__ << "[" << this << "] "
          << " id: " << decoder_id_;
  {
    std::lock_guard<std::mutex> lock(webrtc_video_decoder_lock_);
    WebRtcDecoderMap::iterator it = webrtc_video_decoder_map_.find(decoder_id_);
    if (it != webrtc_video_decoder_map_.end())
      webrtc_video_decoder_map_.erase(it);
  }

  Release();
}

int32_t WebRtcPassThroughVideoDecoder::InitDecode(
    const webrtc::VideoCodec* codec_settings,
    int32_t number_of_cores) {
  VLOG(1) << __func__ << " codec: " << GetCodecName(video_codec_);

  if (codec_settings == nullptr)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

  int ret_val = Release();
  if (ret_val != WEBRTC_VIDEO_CODEC_OK)
    return ret_val;

  propagation_cnt_ = -1;
  initialized_ = true;

  start_timestamp_ = base::TimeTicks::Now();

  // Always start with a complete key frame.
  key_frame_required_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebRtcPassThroughVideoDecoder::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    int64_t render_time_ms) {
  if (!initialized_ || decode_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (media_decoder_acquired_) {
    if (client_ && !client_->HasAvailableResources()) {
      // Fallback to software mode if no free media decoder.
      LOG(INFO) << __func__ << " Decoder fallback to s/w, id: " << decoder_id_;
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  }

  gfx::Size frame_size(input_image._encodedWidth, input_image._encodedHeight);

  if (input_image.data() == NULL && input_image.size() > 0) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (input_image._frameType != webrtc::VideoFrameType::kVideoFrameKey)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    if (input_image._completeFrame) {
      key_frame_required_ = false;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  // Restrict error propagation using key frame requests.
  // Reset on a key frame refresh.
  if (input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey &&
      input_image._completeFrame) {
    propagation_cnt_ = -1;
    // Start count on first loss.
  } else if ((!input_image._completeFrame || missing_frames) &&
             propagation_cnt_ == -1) {
    propagation_cnt_ = 0;
  }
  if (propagation_cnt_ >= 0) {
    propagation_cnt_++;
  }

  int ret = ReturnEncodedFrame(input_image);
  if (ret != 0) {
    // Reset to avoid requesting key frames too often.
    if (ret < 0 && propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return ret;
  }

  // Check Vs. threshold
  if (propagation_cnt_ > kVp8ErrorPropagationTh) {
    // Reset to avoid requesting key frames too often.
    propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebRtcPassThroughVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebRtcPassThroughVideoDecoder::Release() {
  VLOG(1) << __func__;

  media_decoder_acquired_ = false;
  key_frame_required_ = true;

  initialized_ = false;

  return WEBRTC_VIDEO_CODEC_OK;
}

const char* WebRtcPassThroughVideoDecoder::ImplementationName() const {
  return kImplementationName;
}

void WebRtcPassThroughVideoDecoder::RequestKeyFrame() {
  key_frame_required_ = true;
}

void WebRtcPassThroughVideoDecoder::SetClient(Client* client) {
  client_ = client;
}

int32_t WebRtcPassThroughVideoDecoder::ReturnEncodedFrame(
    const webrtc::EncodedImage& input_image) {
  if (input_image.size() == 0) {
    // Decoder OK and NULL image => No show frame
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  bool key_frame =
      input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey;
  if (key_frame) {
    frame_size_.set_width(input_image._encodedWidth);
    frame_size_.set_height(input_image._encodedHeight);
    VLOG(1) << __func__ << " key_frame size: " << frame_size_.ToString();
  }

  std::unique_ptr<uint8_t, base::AlignedFreeDeleter> encoded_data(
      static_cast<uint8_t*>(base::AlignedAlloc(
          input_image.size(),
          media::VideoFrameLayout::kBufferAddressAlignment)));
  memcpy(encoded_data.get(), input_image.data(), input_image.size());

  // Convert timestamp from 90KHz to ms.
  base::TimeDelta timestamp_ms = base::TimeDelta::FromInternalValue(
      base::checked_cast<uint64_t>(input_image.Timestamp()) * 1000 / 90);

  // Make a shallow copy.
  scoped_refptr<media::VideoFrame> encoded_frame =
      media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_I420, frame_size_, gfx::Rect(frame_size_),
          frame_size_, encoded_data.get(), input_image.size(), timestamp_ms);

  if (!encoded_frame)
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;

  // The bind ensures that we keep a pointer to the encoded data.
  encoded_frame->AddDestructionObserver(
      base::Bind(&base::AlignedFree, encoded_data.release()));
  encoded_frame->metadata()->SetBoolean(media::VideoFrameMetadata::KEY_FRAME,
                                        key_frame);
  encoded_frame->metadata()->SetInteger(media::VideoFrameMetadata::CODEC_ID,
                                        video_codec_);

  if (!media_decoder_acquired_) {
    encoded_frame->set_decoder_id(decoder_id_);
    media_decoder_acquired_ = true;
  }

  webrtc::VideoFrame rtc_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(
              new rtc::RefCountedObject<blink::WebRtcVideoFrameAdapter>(
                  std::move(encoded_frame),
                  blink::WebRtcVideoFrameAdapter::LogStatus::kNoLogging))
          .set_timestamp_rtp(input_image.Timestamp())
          .set_rotation(webrtc::kVideoRotation_0)
          .build();
  rtc_frame.set_timestamp(input_image.Timestamp());
  rtc_frame.set_ntp_time_ms(input_image.ntp_time_ms_);

  decode_complete_callback_->Decoded(rtc_frame, absl::nullopt, 0);

  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace media
