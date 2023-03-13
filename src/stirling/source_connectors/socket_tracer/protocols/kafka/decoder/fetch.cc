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
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/message_set.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

StatusOr<FetchReqTopic> PacketDecoder::ExtractFetchReqTopic() {
  FetchReqTopic r;
  PX_ASSIGN_OR_RETURN(r.name, ExtractString());
  PX_ASSIGN_OR_RETURN(r.partitions,
                      ExtractArray<FetchReqPartition>(&PacketDecoder::ExtractFetchReqPartition));
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<FetchReqPartition> PacketDecoder::ExtractFetchReqPartition() {
  FetchReqPartition r;
  PX_ASSIGN_OR_RETURN(r.index, ExtractInt32());
  if (api_version_ >= 9) {
    PX_ASSIGN_OR_RETURN(r.current_leader_epoch, ExtractInt32());
  }
  PX_ASSIGN_OR_RETURN(r.fetch_offset, ExtractInt64());
  if (api_version_ >= 12) {
    PX_ASSIGN_OR_RETURN(r.last_fetched_epoch, ExtractInt32());
  }
  if (api_version_ >= 5) {
    PX_ASSIGN_OR_RETURN(r.log_start_offset, ExtractInt64());
  }
  PX_ASSIGN_OR_RETURN(r.partition_max_bytes, ExtractInt32());
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<FetchForgottenTopicsData> PacketDecoder::ExtractFetchForgottenTopicsData() {
  FetchForgottenTopicsData r;
  PX_ASSIGN_OR_RETURN(r.name, ExtractString());
  PX_ASSIGN_OR_RETURN(r.partition_indices, ExtractArray<int32_t>(&PacketDecoder::ExtractInt32));
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<FetchReq> PacketDecoder::ExtractFetchReq() {
  FetchReq r;
  PX_ASSIGN_OR_RETURN(r.replica_id, ExtractInt32());
  PX_RETURN_IF_ERROR(/*max_wait_ms*/ ExtractInt32());
  PX_RETURN_IF_ERROR(/*min_bytes*/ ExtractInt32());

  if (api_version_ >= 3) {
    PX_RETURN_IF_ERROR(/*max_bytes*/ ExtractInt32());
  }

  if (api_version_ >= 4) {
    PX_RETURN_IF_ERROR(/*isolation_level*/ ExtractInt8());
  }

  if (api_version_ >= 7) {
    PX_ASSIGN_OR_RETURN(r.session_id, ExtractInt32());
    PX_ASSIGN_OR_RETURN(r.session_epoch, ExtractInt32());
  }

  PX_ASSIGN_OR_RETURN(r.topics, ExtractArray<FetchReqTopic>(&PacketDecoder::ExtractFetchReqTopic));

  if (api_version_ >= 7) {
    PX_ASSIGN_OR_RETURN(r.forgotten_topics, ExtractArray<FetchForgottenTopicsData>(
                                                &PacketDecoder::ExtractFetchForgottenTopicsData));
  }

  if (api_version_ >= 11) {
    PX_ASSIGN_OR_RETURN(r.rack_id, ExtractString());
  }

  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<FetchRespAbortedTransaction> PacketDecoder::ExtractFetchRespAbortedTransaction() {
  FetchRespAbortedTransaction r;
  PX_ASSIGN_OR_RETURN(r.producer_id, ExtractInt64());
  PX_ASSIGN_OR_RETURN(r.first_offset, ExtractInt64());
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<FetchRespPartition> PacketDecoder::ExtractFetchRespPartition() {
  FetchRespPartition r;

  PX_ASSIGN_OR_RETURN(r.index, ExtractInt32());
  PX_ASSIGN_OR_RETURN(r.error_code, ExtractInt16());
  PX_ASSIGN_OR_RETURN(r.high_watermark, ExtractInt64());
  if (api_version_ >= 4) {
    PX_ASSIGN_OR_RETURN(r.last_stable_offset, ExtractInt64());
  }
  if (api_version_ >= 5) {
    PX_ASSIGN_OR_RETURN(r.log_start_offset, ExtractInt64());
  }
  if (api_version_ >= 4) {
    PX_ASSIGN_OR_RETURN(r.aborted_transactions,
                        ExtractArray(&PacketDecoder::ExtractFetchRespAbortedTransaction));
  }
  if (api_version_ >= 11) {
    PX_ASSIGN_OR_RETURN(r.preferred_read_replica, ExtractInt32());
  }
  PX_ASSIGN_OR_RETURN(r.message_set, ExtractMessageSet());
  // No tag section here, since it's been handled in MessageSet.
  return r;
}

StatusOr<FetchRespTopic> PacketDecoder::ExtractFetchRespTopic() {
  FetchRespTopic r;
  PX_ASSIGN_OR_RETURN(r.name, ExtractString());
  PX_ASSIGN_OR_RETURN(r.partitions, ExtractArray(&PacketDecoder::ExtractFetchRespPartition));
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<FetchResp> PacketDecoder::ExtractFetchResp() {
  FetchResp r;
  if (api_version_ >= 1) {
    PX_ASSIGN_OR_RETURN(r.throttle_time_ms, ExtractInt32());
  }
  if (api_version_ >= 7) {
    PX_ASSIGN_OR_RETURN(r.error_code, ExtractInt16());
    PX_ASSIGN_OR_RETURN(r.session_id, ExtractInt32());
  }
  PX_ASSIGN_OR_RETURN(r.topics, ExtractArray(&PacketDecoder::ExtractFetchRespTopic));
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
