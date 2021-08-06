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
  PL_ASSIGN_OR_RETURN(r.index, ExtractInt32());

  // COMPACT_RECORDS is used in api_version >= 9.
  PL_ASSIGN_OR_RETURN(r.message_set, ExtractMessageSet());
  return r;
}

StatusOr<ProduceReqTopic> PacketDecoder::ExtractProduceReqTopic() {
  ProduceReqTopic r;
  PL_ASSIGN_OR_RETURN(r.name, ExtractString());
  PL_ASSIGN_OR_RETURN(r.partitions, ExtractArray(&PacketDecoder::ExtractProduceReqPartition));
  PL_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<RecordError> PacketDecoder::ExtractRecordError() {
  RecordError r;

  PL_ASSIGN_OR_RETURN(r.batch_index, ExtractInt32());
  PL_ASSIGN_OR_RETURN(r.error_message, ExtractNullableString());
  PL_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<ProduceRespPartition> PacketDecoder::ExtractProduceRespPartition() {
  ProduceRespPartition r;

  PL_ASSIGN_OR_RETURN(r.index, ExtractInt32());
  PL_ASSIGN_OR_RETURN(r.error_code, ExtractInt16());
  PL_ASSIGN_OR_RETURN(int64_t base_offset, ExtractInt64());
  if (api_version_ >= 2) {
    PL_ASSIGN_OR_RETURN(int64_t log_append_time_ms, ExtractInt64());
    PL_UNUSED(log_append_time_ms);
  }
  if (api_version_ >= 5) {
    PL_ASSIGN_OR_RETURN(int64_t log_start_offset, ExtractInt64());
    PL_UNUSED(log_start_offset);
  }
  PL_ASSIGN_OR_RETURN(r.record_errors, ExtractArray(&PacketDecoder::ExtractRecordError));
  PL_ASSIGN_OR_RETURN(r.error_message, ExtractNullableString());
  PL_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());

  PL_UNUSED(base_offset);
  return r;
}

StatusOr<ProduceRespTopic> PacketDecoder::ExtractProduceRespTopic() {
  ProduceRespTopic r;

  if (is_flexible_) {
    PL_ASSIGN_OR_RETURN(r.name, ExtractCompactString());
    PL_ASSIGN_OR_RETURN(r.partitions,
                        ExtractCompactArray(&PacketDecoder::ExtractProduceRespPartition));
    PL_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  } else {
    PL_ASSIGN_OR_RETURN(r.name, ExtractString());
    PL_ASSIGN_OR_RETURN(r.partitions, ExtractArray(&PacketDecoder::ExtractProduceRespPartition));
  }
  return r;
}

// Documentation: https://kafka.apache.org/protocol.html#The_Messages_Produce
StatusOr<ProduceReq> PacketDecoder::ExtractProduceReq() {
  ProduceReq r;
  if (is_flexible_) {
    PL_ASSIGN_OR_RETURN(r.transactional_id, ExtractCompactNullableString());
  } else if (api_version_ >= 3) {
    PL_ASSIGN_OR_RETURN(r.transactional_id, ExtractNullableString());
  }
  PL_ASSIGN_OR_RETURN(r.acks, ExtractInt16());
  PL_ASSIGN_OR_RETURN(r.timeout_ms, ExtractInt32());
  if (is_flexible_) {
    PL_ASSIGN_OR_RETURN(r.topics, ExtractCompactArray(&PacketDecoder::ExtractProduceReqTopic));
  } else {
    PL_ASSIGN_OR_RETURN(r.topics, ExtractArray(&PacketDecoder::ExtractProduceReqTopic));
  }
  return r;
}

StatusOr<ProduceResp> PacketDecoder::ExtractProduceResp() {
  ProduceResp r;

  if (is_flexible_) {
    PL_ASSIGN_OR_RETURN(r.topics, ExtractCompactArray(&PacketDecoder::ExtractProduceRespTopic));
  } else {
    PL_ASSIGN_OR_RETURN(r.topics, ExtractArray(&PacketDecoder::ExtractProduceRespTopic));
  }

  if (api_version_ >= 1) {
    PL_ASSIGN_OR_RETURN(r.throttle_time_ms, ExtractInt32());
  }

  if (is_flexible_) {
    PL_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  }
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
