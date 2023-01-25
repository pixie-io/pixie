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
  PX_ASSIGN_OR_RETURN(int32_t length, ExtractVarint());
  PX_RETURN_IF_ERROR(MarkOffset(length));

  PX_ASSIGN_OR_RETURN(int8_t attributes, ExtractInt8());
  PX_ASSIGN_OR_RETURN(int64_t timestamp_delta, ExtractVarlong());
  PX_ASSIGN_OR_RETURN(int32_t offset_delta, ExtractVarint());
  PX_ASSIGN_OR_RETURN(r.key, ExtractBytesZigZag());
  PX_ASSIGN_OR_RETURN(r.value, ExtractBytesZigZag());

  PX_UNUSED(attributes);
  PX_UNUSED(timestamp_delta);
  PX_UNUSED(offset_delta);

  // Discard record headers and jump to the marked offset.
  PX_RETURN_IF_ERROR(JumpToOffset());
  return r;
}

// Only supports Kafka version >= 0.11.0
StatusOr<RecordBatch> PacketDecoder::ExtractRecordBatch(int32_t* offset) {
  constexpr int32_t kBaseOffsetLength = 8;
  constexpr int32_t kLengthLength = 4;

  RecordBatch r;
  PX_ASSIGN_OR_RETURN(int64_t base_offset, ExtractInt64());

  PX_ASSIGN_OR_RETURN(int32_t length, ExtractInt32());
  PX_RETURN_IF_ERROR(MarkOffset(length));

  PX_ASSIGN_OR_RETURN(int32_t partition_leader_epoch, ExtractInt32());
  PX_ASSIGN_OR_RETURN(int8_t magic, ExtractInt8());
  // If magic is not v2, then this is potentially an older format.
  if (magic < 2) {
    return error::Internal("Old record batch (message set) format not supported.");
  }
  if (magic > 2) {
    return error::Internal("Unknown magic in ExtractRecordBatch.");
  }

  PX_ASSIGN_OR_RETURN(int32_t crc, ExtractInt32());
  PX_ASSIGN_OR_RETURN(int16_t attributes, ExtractInt16());
  PX_ASSIGN_OR_RETURN(int32_t last_offset_delta, ExtractInt32());
  PX_ASSIGN_OR_RETURN(int64_t first_time_stamp, ExtractInt64());
  PX_ASSIGN_OR_RETURN(int64_t max_time_stamp, ExtractInt64());
  PX_ASSIGN_OR_RETURN(int64_t producer_ID, ExtractInt64());
  PX_ASSIGN_OR_RETURN(int16_t producer_epoch, ExtractInt16());
  PX_ASSIGN_OR_RETURN(int32_t base_sequence, ExtractInt32());

  PX_UNUSED(base_offset);
  PX_UNUSED(partition_leader_epoch);
  PX_UNUSED(crc);
  PX_UNUSED(attributes);
  PX_UNUSED(last_offset_delta);
  PX_UNUSED(first_time_stamp);
  PX_UNUSED(max_time_stamp);
  PX_UNUSED(producer_ID);
  PX_UNUSED(producer_epoch);
  PX_UNUSED(base_sequence);

  PX_ASSIGN_OR_RETURN(r.records, ExtractRegularArray(&PacketDecoder::ExtractRecordMessage));
  PX_RETURN_IF_ERROR(JumpToOffset());

  *offset += length + kBaseOffsetLength + kLengthLength;
  return r;
}

StatusOr<MessageSet> PacketDecoder::ExtractMessageSet() {
  MessageSet message_set;

  int32_t offset = 0;

  if (is_flexible_) {
    PX_ASSIGN_OR_RETURN(message_set.size, ExtractUnsignedVarint());
  } else {
    PX_ASSIGN_OR_RETURN(message_set.size, ExtractInt32());
  }
  PX_RETURN_IF_ERROR(MarkOffset(message_set.size));

  // The message set in a fetch response is sent with the sendfile syscall:
  // sendfile(int out_fd, int in_fd, off_t *offset, size_t count). We can only get the length of
  // the payload, not the content. To make sure ParseFrame functions correctly, a temporary fix
  // was put in to fill the content of sendfile syscall (a.k.a the message set) with zeros. In this
  // case, since parsing of the record batches are best effort anyways, we do jump over to the end
  // when ExtractRecordBatch fails.

  // There is no field in the message set that indicates how many record batches should follow.
  // The length parsed above contains the length of the message set and the tagged section, if there
  // is one (in flexible versions). This makes difficult to determine where parsing of the record
  // batches should end. Thus, a best effort parsing is used here to parse out as many
  // record batches as possible. If an error occurs due to tagged section, we just jump to the
  // correct offset and continue parsing.

  while (offset < message_set.size) {
    auto record_batch_result = ExtractRecordBatch(&offset);
    if (record_batch_result.ok()) {
      message_set.record_batches.push_back(record_batch_result.ValueOrDie());
    } else {
      PX_RETURN_IF_ERROR(JumpToOffset());
      return message_set;
    }
  }

  if (is_flexible_) {
    PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  }

  PX_RETURN_IF_ERROR(JumpToOffset());
  return message_set;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
