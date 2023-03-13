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

StatusOr<ProduceReqPartition> PacketDecoder::ExtractProduceReqPartition() {
  ProduceReqPartition r;
  PX_ASSIGN_OR_RETURN(r.index, ExtractInt32());

  // COMPACT_RECORDS is used in api_version >= 9.
  PX_ASSIGN_OR_RETURN(r.message_set, ExtractMessageSet());
  return r;
}

StatusOr<ProduceReqTopic> PacketDecoder::ExtractProduceReqTopic() {
  ProduceReqTopic r;
  PX_ASSIGN_OR_RETURN(r.name, ExtractString());
  PX_ASSIGN_OR_RETURN(r.partitions, ExtractArray(&PacketDecoder::ExtractProduceReqPartition));
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<RecordError> PacketDecoder::ExtractRecordError() {
  RecordError r;

  PX_ASSIGN_OR_RETURN(r.batch_index, ExtractInt32());
  PX_ASSIGN_OR_RETURN(r.error_message, ExtractNullableString());
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<ProduceRespPartition> PacketDecoder::ExtractProduceRespPartition() {
  ProduceRespPartition r;

  PX_ASSIGN_OR_RETURN(r.index, ExtractInt32());
  PX_ASSIGN_OR_RETURN(r.error_code, ExtractInt16());
  PX_ASSIGN_OR_RETURN(r.base_offset, ExtractInt64());
  if (api_version_ >= 2) {
    PX_ASSIGN_OR_RETURN(r.log_append_time_ms, ExtractInt64());
  }
  if (api_version_ >= 5) {
    PX_ASSIGN_OR_RETURN(r.log_start_offset, ExtractInt64());
  }
  if (api_version_ >= 8) {
    PX_ASSIGN_OR_RETURN(r.record_errors, ExtractArray(&PacketDecoder::ExtractRecordError));
    PX_ASSIGN_OR_RETURN(r.error_message, ExtractNullableString());
  }
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());

  return r;
}

StatusOr<ProduceRespTopic> PacketDecoder::ExtractProduceRespTopic() {
  ProduceRespTopic r;

  if (is_flexible_) {
    PX_ASSIGN_OR_RETURN(r.name, ExtractCompactString());
    PX_ASSIGN_OR_RETURN(r.partitions,
                        ExtractCompactArray(&PacketDecoder::ExtractProduceRespPartition));
    PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  } else {
    PX_ASSIGN_OR_RETURN(r.name, ExtractString());
    PX_ASSIGN_OR_RETURN(r.partitions, ExtractArray(&PacketDecoder::ExtractProduceRespPartition));
  }
  return r;
}

// Documentation: https://kafka.apache.org/protocol.html#The_Messages_Produce
StatusOr<ProduceReq> PacketDecoder::ExtractProduceReq() {
  ProduceReq r;
  if (is_flexible_) {
    PX_ASSIGN_OR_RETURN(r.transactional_id, ExtractCompactNullableString());
  } else if (api_version_ >= 3) {
    PX_ASSIGN_OR_RETURN(r.transactional_id, ExtractNullableString());
  }
  PX_ASSIGN_OR_RETURN(r.acks, ExtractInt16());
  PX_ASSIGN_OR_RETURN(r.timeout_ms, ExtractInt32());
  if (is_flexible_) {
    PX_ASSIGN_OR_RETURN(r.topics, ExtractCompactArray(&PacketDecoder::ExtractProduceReqTopic));
  } else {
    PX_ASSIGN_OR_RETURN(r.topics, ExtractArray(&PacketDecoder::ExtractProduceReqTopic));
  }
  return r;
}

StatusOr<ProduceResp> PacketDecoder::ExtractProduceResp() {
  ProduceResp r;

  if (is_flexible_) {
    PX_ASSIGN_OR_RETURN(r.topics, ExtractCompactArray(&PacketDecoder::ExtractProduceRespTopic));
  } else {
    PX_ASSIGN_OR_RETURN(r.topics, ExtractArray(&PacketDecoder::ExtractProduceRespTopic));
  }

  if (api_version_ >= 1) {
    PX_ASSIGN_OR_RETURN(r.throttle_time_ms, ExtractInt32());
  }

  if (is_flexible_) {
    PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  }
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
