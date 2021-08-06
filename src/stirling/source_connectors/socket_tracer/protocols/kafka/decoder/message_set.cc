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

// Only supports Kafka version >= 0.11.0
StatusOr<RecordMessage> PacketDecoder::ExtractRecordMessage() {
  RecordMessage r;
  PL_ASSIGN_OR_RETURN(int32_t length, ExtractVarint());
  PL_RETURN_IF_ERROR(MarkOffset(length));

  PL_ASSIGN_OR_RETURN(int8_t attributes, ExtractInt8());
  PL_ASSIGN_OR_RETURN(int64_t timestamp_delta, ExtractVarlong());
  PL_ASSIGN_OR_RETURN(int32_t offset_delta, ExtractVarint());
  PL_ASSIGN_OR_RETURN(r.key, ExtractBytesZigZag());
  PL_ASSIGN_OR_RETURN(r.value, ExtractBytesZigZag());

  PL_UNUSED(attributes);
  PL_UNUSED(timestamp_delta);
  PL_UNUSED(offset_delta);

  // Discard record headers and jump to the marked offset.
  PL_RETURN_IF_ERROR(JumpToOffset());
  return r;
}

// Only supports Kafka version >= 0.11.0
StatusOr<RecordBatch> PacketDecoder::ExtractRecordBatch(int32_t* offset) {
  constexpr int32_t kBaseOffsetLength = 8;
  constexpr int32_t kLengthLength = 4;

  RecordBatch r;
  PL_ASSIGN_OR_RETURN(int64_t base_offset, ExtractInt64());

  PL_ASSIGN_OR_RETURN(int32_t length, ExtractInt32());
  PL_RETURN_IF_ERROR(MarkOffset(length));

  PL_ASSIGN_OR_RETURN(int32_t partition_leader_epoch, ExtractInt32());
  PL_ASSIGN_OR_RETURN(int8_t magic, ExtractInt8());
  // If magic is not v2, then this is potentially an older format.
  if (magic < 2) {
    return error::Internal("Old record batch (message set) format not supported.");
  }
  if (magic > 2) {
    return error::Internal("Unknown magic in ExtractRecordBatch.");
  }

  PL_ASSIGN_OR_RETURN(int32_t crc, ExtractInt32());
  PL_ASSIGN_OR_RETURN(int16_t attributes, ExtractInt16());
  PL_ASSIGN_OR_RETURN(int32_t last_offset_delta, ExtractInt32());
  PL_ASSIGN_OR_RETURN(int64_t first_time_stamp, ExtractInt64());
  PL_ASSIGN_OR_RETURN(int64_t max_time_stamp, ExtractInt64());
  PL_ASSIGN_OR_RETURN(int64_t producer_ID, ExtractInt64());
  PL_ASSIGN_OR_RETURN(int16_t producer_epoch, ExtractInt16());
  PL_ASSIGN_OR_RETURN(int32_t base_sequence, ExtractInt32());

  PL_UNUSED(base_offset);
  PL_UNUSED(partition_leader_epoch);
  PL_UNUSED(crc);
  PL_UNUSED(attributes);
  PL_UNUSED(last_offset_delta);
  PL_UNUSED(first_time_stamp);
  PL_UNUSED(max_time_stamp);
  PL_UNUSED(producer_ID);
  PL_UNUSED(producer_epoch);
  PL_UNUSED(base_sequence);

  PL_ASSIGN_OR_RETURN(r.records, ExtractRegularArray(&PacketDecoder::ExtractRecordMessage));
  PL_RETURN_IF_ERROR(JumpToOffset());

  *offset += length + kBaseOffsetLength + kLengthLength;
  return r;
}

StatusOr<MessageSet> PacketDecoder::ExtractMessageSet() {
  MessageSet message_set;

  int32_t length = 0;
  int32_t offset = 0;

  if (is_flexible_) {
    PL_ASSIGN_OR_RETURN(length, ExtractUnsignedVarint());
  } else {
    PL_ASSIGN_OR_RETURN(length, ExtractInt32());
  }
  PL_RETURN_IF_ERROR(MarkOffset(length));

  // There is no field in the message set that indicates how many record batches should follow.
  // The length parsed above contains the length of the message set and the tagged section, if there
  // is one (in flexible versions). This makes difficult to determine where parsing of the record
  // batches should end. Thus, a best effort parsing is used here to parse out as many
  // record batches as possible. If an error occurs due to tagged section, we just jump to the
  // correct offset and continue parsing.
  while (offset < length) {
    auto record_batch_result = ExtractRecordBatch(&offset);
    if (record_batch_result.ok()) {
      message_set.record_batches.push_back(record_batch_result.ValueOrDie());
    } else {
      PL_RETURN_IF_ERROR(JumpToOffset());
      return message_set;
    }
  }

  if (is_flexible_) {
    PL_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  }

  PL_RETURN_IF_ERROR(JumpToOffset());
  return message_set;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
