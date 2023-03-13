/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/decoder/packet_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

// Topic Data in Metadata Request.
StatusOr<MetadataReqTopic> PacketDecoder::ExtractMetadataReqTopic() {
  MetadataReqTopic r;

  if (api_version_ >= 10) {
    PX_ASSIGN_OR_RETURN(r.topic_id, ExtractString());
  }

  if (api_version_ <= 9) {
    PX_ASSIGN_OR_RETURN(r.name, ExtractString());
  } else {
    PX_ASSIGN_OR_RETURN(r.name, ExtractNullableString());
  }

  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<MetadataReq> PacketDecoder::ExtractMetadataReq() {
  MetadataReq r;

  PX_ASSIGN_OR_RETURN(r.topics, ExtractArray(&PacketDecoder::ExtractMetadataReqTopic));
  if (api_version_ >= 4) {
    PX_ASSIGN_OR_RETURN(r.allow_auto_topic_creation, ExtractBool());
  }

  if (api_version_ >= 8) {
    PX_ASSIGN_OR_RETURN(r.include_cluster_authorized_operations, ExtractBool());
    PX_ASSIGN_OR_RETURN(r.include_topic_authorized_operations, ExtractBool());
  }
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
