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

#include "media/neva/webos/svp_util.h"

#include "base/logging.h"

#if defined(ENABLE_LG_SVP)
#include <svp/inband/inband_serializer.h>
#endif

namespace media {

scoped_refptr<DecoderBuffer> MakeSvpInBandHeader(
    const scoped_refptr<DecoderBuffer>& buffer) {
#if defined(ENABLE_LG_SVP)
  CHECK(!buffer->decrypt_config());

  if (buffer->end_of_stream()) {
    return buffer.get();
  }

  svp::InbandSerializer<bool> inband_serializer;
  std::vector<uint8_t> serialized_data;

  if (!inband_serializer.CanDoSerialize()) {
    LOG(ERROR) << "inband_serializer for <bool> is not supported";
    return nullptr;
  }

  // First argument 'false' means this buffer isn't encrypted
  if (!inband_serializer.Configure(false, buffer->data(),
                                   buffer->data_size())) {
    LOG(ERROR) << "Failed to inband_serializer.Configure(false)";
    return nullptr;
  }

  if (!inband_serializer.GetSerialized(&serialized_data)) {
    LOG(ERROR) << "Failed to inband_serializer.GetSerialized()";
    return nullptr;
  }

  scoped_refptr<DecoderBuffer> out_buffer =
      DecoderBuffer::CopyFrom(serialized_data.data(), serialized_data.size());
  out_buffer->set_timestamp(buffer->timestamp());
  out_buffer->set_duration(buffer->duration());
  out_buffer->set_use_svp_serialized_data(true);

  return out_buffer.get();
#else
  return buffer.get();
#endif  // defined(ENABLE_LG_SVP)
}

}  // namespace media
