// Copyright 2019 LG Electronics, Inc.
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

#include "media/neva/webos/umedia_info_util_webos_smp.h"

#include "base/logging.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#define FUNC_LOG(x) DVLOG(x) << __func__

namespace media {

std::string SourceInfoToJson(const std::string& media_id,
                             const struct uMediaServer::source_info_t& value) {
  Json::Value eventInfo;
  Json::Value sourceInfo;
  Json::FastWriter writer;
  std::string res;

  eventInfo["type"] = "sourceInfo";
  eventInfo["mediaId"] = media_id.c_str();

  sourceInfo["container"] = value.container.c_str();
  sourceInfo["seekable"] = value.seekable;
  sourceInfo["trickable"] = value.trickable;
  sourceInfo["rotation"] = value.rotation;
  sourceInfo["numPrograms"] = value.numPrograms;

  Json::Value programInfos(Json::arrayValue);
  for (int i = 0; i < value.numPrograms; i++) {
    Json::Value programInfo;
    programInfo["duration"] =
        static_cast<int64_t>(value.programInfo[i].duration);
    programInfo["numAudioTracks"] = value.programInfo[i].numAudioTracks;
    Json::Value audioTrackInfos(Json::arrayValue);
    for (int j = 0; j < value.programInfo[i].numAudioTracks; j++) {
      Json::Value audioTrackInfo;
      audioTrackInfo["language"] =
          value.programInfo[i].audioTrackInfo[j].language.c_str();
      audioTrackInfo["codec"] =
          value.programInfo[i].audioTrackInfo[j].codec.c_str();
      audioTrackInfo["profile"] =
          value.programInfo[i].audioTrackInfo[j].profile.c_str();
      audioTrackInfo["level"] =
          value.programInfo[i].audioTrackInfo[j].level.c_str();
      audioTrackInfo["bitRate"] =
          value.programInfo[i].audioTrackInfo[j].bitRate;
      audioTrackInfo["sampleRate"] =
          value.programInfo[i].audioTrackInfo[j].sampleRate;
      audioTrackInfo["channels"] =
          value.programInfo[i].audioTrackInfo[j].channels;
#if defined(USE_TV_MEDIA)
      audioTrackInfo["pid"] = value.programInfo[i].audioTrackInfo[j].pid;
      audioTrackInfo["ctag"] = value.programInfo[i].audioTrackInfo[j].ctag;
      audioTrackInfo["audioDescription"] =
          value.programInfo[i].audioTrackInfo[j].audioDescription;
      audioTrackInfo["audioType"] =
          value.programInfo[i].audioTrackInfo[j].audioType;
      audioTrackInfo["trackId"] =
          value.programInfo[i].audioTrackInfo[j].trackId;
      audioTrackInfo["role"] =
          value.programInfo[i].audioTrackInfo[j].role.c_str();
      audioTrackInfo["adaptationSetId"] =
          value.programInfo[i].audioTrackInfo[j].adaptationSetId;
#endif
      audioTrackInfos.append(audioTrackInfo);
    }
    if (value.programInfo[i].numAudioTracks)
      programInfo["audioTrackInfo"] = audioTrackInfos;

    programInfo["numVideoTracks"] = value.programInfo[i].numVideoTracks;
    Json::Value videoTrackInfos(Json::arrayValue);
    for (int j = 0; j < value.programInfo[i].numVideoTracks; j++) {
      Json::Value videoTrackInfo;
      videoTrackInfo["angleNumber"] =
          value.programInfo[i].videoTrackInfo[j].angleNumber;
      videoTrackInfo["codec"] =
          value.programInfo[i].videoTrackInfo[j].codec.c_str();
      videoTrackInfo["profile"] =
          value.programInfo[i].videoTrackInfo[j].profile.c_str();
      videoTrackInfo["level"] =
          value.programInfo[i].videoTrackInfo[j].level.c_str();
      videoTrackInfo["width"] = value.programInfo[i].videoTrackInfo[j].width;
      videoTrackInfo["height"] = value.programInfo[i].videoTrackInfo[j].height;
      videoTrackInfo["aspectRatio"] =
          value.programInfo[i].videoTrackInfo[j].aspectRatio.c_str();
      videoTrackInfo["frameRate"] =
          value.programInfo[i].videoTrackInfo[j].frameRate;
      videoTrackInfo["bitRate"] =
          value.programInfo[i].videoTrackInfo[j].bitRate;
      videoTrackInfo["progressive"] =
          value.programInfo[i].videoTrackInfo[j].progressive;
#if defined(USE_TV_MEDIA)
      videoTrackInfo["pid"] = value.programInfo[i].videoTrackInfo[j].pid;
      videoTrackInfo["ctag"] = value.programInfo[i].videoTrackInfo[j].ctag;
      videoTrackInfo["trackId"] =
          value.programInfo[i].videoTrackInfo[j].trackId;
      videoTrackInfo["role"] =
          value.programInfo[i].videoTrackInfo[j].role.c_str();
      videoTrackInfo["adaptationSetId"] =
          value.programInfo[i].videoTrackInfo[j].adaptationSetId;
      videoTrackInfo["orgCodec"] =
          value.programInfo[i].videoTrackInfo[j].orgCodec.c_str();
#endif
      videoTrackInfos.append(videoTrackInfo);
    }
    if (value.programInfo[i].numVideoTracks)
      programInfo["videoTrackInfo"] = videoTrackInfos;

    programInfo["numSubtitleTracks"] = value.programInfo[i].numSubtitleTracks;
    Json::Value subtitleTrackInfos(Json::arrayValue);
    for (int j = 0; j < value.programInfo[i].numSubtitleTracks; j++) {
      Json::Value subtitleTrackInfo;
      subtitleTrackInfo["language"] =
          value.programInfo[i].subtitleTrackInfo[j].language.c_str();
#if defined(USE_TV_MEDIA)
      subtitleTrackInfo["pid"] = value.programInfo[i].subtitleTrackInfo[j].pid;
      subtitleTrackInfo["ctag"] =
          value.programInfo[i].subtitleTrackInfo[j].ctag;
      subtitleTrackInfo["type"] =
          value.programInfo[i].subtitleTrackInfo[j].type;
      subtitleTrackInfo["compositionPageId"] =
          value.programInfo[i].subtitleTrackInfo[j].compositionPageId;
      subtitleTrackInfo["ancilaryPageId"] =
          value.programInfo[i].subtitleTrackInfo[j].ancilaryPageId;
      subtitleTrackInfo["hearingImpared"] =
          value.programInfo[i].subtitleTrackInfo[j].hearingImpared;
      subtitleTrackInfo["trackId"] =
          value.programInfo[i].subtitleTrackInfo[j].trackId;
#endif
      subtitleTrackInfos.append(subtitleTrackInfo);
    }
    if (value.programInfo[i].numSubtitleTracks)
      programInfo["subtitleTrackInfo"] = subtitleTrackInfos;

    programInfos.append(programInfo);
  }
  sourceInfo["programInfo"] = programInfos;

  eventInfo["info"] = sourceInfo;
  res = writer.write(eventInfo);
  return res;
}

std::string VideoInfoToJson(const std::string& media_id,
                            const struct uMediaServer::video_info_t& value) {
  Json::Value eventInfo;
  Json::Value videoInfo;
  Json::FastWriter writer;
  std::string res;

  eventInfo["type"] = "videoInfo";
  eventInfo["mediaId"] = media_id.c_str();

  videoInfo["aspectRatio"] = value.aspectRatio.c_str();
  videoInfo["bitRate"] = value.bitRate;
  videoInfo["width"] = value.width;
  videoInfo["height"] = value.height;
  videoInfo["frameRate"] = value.frameRate;
  videoInfo["mode3D"] = value.mode3D.c_str();
  videoInfo["actual3D"] = value.actual3D.c_str();
  videoInfo["scanType"] = value.scanType.c_str();
  videoInfo["pixelAspectRatio"] = value.pixelAspectRatio.c_str();

  if (value.isValidSeiInfo) {
    videoInfo["SEI"]["displayPrimariesX0"] = value.SEI.displayPrimariesX0;
    videoInfo["SEI"]["displayPrimariesX1"] = value.SEI.displayPrimariesX1;
    videoInfo["SEI"]["displayPrimariesX2"] = value.SEI.displayPrimariesX2;
    videoInfo["SEI"]["displayPrimariesY0"] = value.SEI.displayPrimariesY0;
    videoInfo["SEI"]["displayPrimariesY1"] = value.SEI.displayPrimariesY1;
    videoInfo["SEI"]["displayPrimariesY2"] = value.SEI.displayPrimariesY2;
    videoInfo["SEI"]["whitePointX"] = value.SEI.whitePointX;
    videoInfo["SEI"]["whitePointY"] = value.SEI.whitePointY;
    videoInfo["SEI"]["minDisplayMasteringLuminance"] =
        value.SEI.minDisplayMasteringLuminance;
    videoInfo["SEI"]["maxDisplayMasteringLuminance"] =
        value.SEI.maxDisplayMasteringLuminance;
  }

  if (value.isValidVuiInfo) {
    videoInfo["VUI"]["transferCharacteristics"] =
        value.VUI.transferCharacteristics;
    videoInfo["VUI"]["colorPrimaries"] = value.VUI.colorPrimaries;
    videoInfo["VUI"]["matrixCoeffs"] = value.VUI.matrixCoeffs;
  }

  videoInfo["hdrType"] = value.hdrType;

#if defined(USE_TV_MEDIA)
  videoInfo["rotation"] = value.rotation;
#endif

  eventInfo["info"] = videoInfo;
  res = writer.write(eventInfo);
  return res;
}

std::string AudioInfoToJson(const std::string& media_id,
                            const struct uMediaServer::audio_info_t& value) {
  Json::Value eventInfo;
  Json::Value audioInfo;
  Json::FastWriter writer;

  eventInfo["type"] = "audioInfo";
  eventInfo["mediaId"] = media_id.c_str();

#if defined(USE_TV_MEDIA)
  audioInfo["dualMono"] = value.dualMono;
  audioInfo["track"] = value.track;
#endif
  audioInfo["sampleRate"] = value.sampleRate;
  audioInfo["channels"] = value.channels;
#if defined(USE_TV_MEDIA)
  audioInfo["immersive"] = value.immersive;
#endif

  eventInfo["info"] = audioInfo;

  return writer.write(eventInfo);
}

}  // namespace media
