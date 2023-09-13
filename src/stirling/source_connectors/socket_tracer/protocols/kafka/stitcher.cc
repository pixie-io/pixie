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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/stitcher.h"

#include <absl/container/flat_hash_map.h>
#include <deque>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/decoder/packet_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

Status ProcessProduceReq(PacketDecoder* decoder, Request* req) {
  PX_ASSIGN_OR_RETURN(ProduceReq r, decoder->ExtractProduceReq());

  req->msg = ToString(r);
  return Status::OK();
}

Status ProcessProduceResp(PacketDecoder* decoder, Response* resp) {
  PX_ASSIGN_OR_RETURN(ProduceResp r, decoder->ExtractProduceResp());

  resp->msg = ToString(r);
  return Status::OK();
}

Status ProcessFetchReq(PacketDecoder* decoder, Request* req) {
  PX_ASSIGN_OR_RETURN(FetchReq r, decoder->ExtractFetchReq());

  req->msg = ToString(r);
  return Status::OK();
}

Status ProcessFetchResp(PacketDecoder* decoder, Response* resp) {
  PX_ASSIGN_OR_RETURN(FetchResp r, decoder->ExtractFetchResp());

  resp->msg = ToString(r);
  return Status::OK();
}

Status ProcessJoinGroupReq(PacketDecoder* decoder, Request* req) {
  PX_ASSIGN_OR_RETURN(JoinGroupReq r, decoder->ExtractJoinGroupReq());

  req->msg = ToString(r);
  return Status::OK();
}

Status ProcessJoinGroupResp(PacketDecoder* decoder, Response* resp) {
  PX_ASSIGN_OR_RETURN(JoinGroupResp r, decoder->ExtractJoinGroupResp());

  resp->msg = ToString(r);
  return Status::OK();
}

Status ProcessSyncGroupReq(PacketDecoder* decoder, Request* req) {
  PX_ASSIGN_OR_RETURN(SyncGroupReq r, decoder->ExtractSyncGroupReq());

  req->msg = ToString(r);
  return Status::OK();
}

Status ProcessSyncGroupResp(PacketDecoder* decoder, Response* resp) {
  PX_ASSIGN_OR_RETURN(SyncGroupResp r, decoder->ExtractSyncGroupResp());

  resp->msg = ToString(r);
  return Status::OK();
}

Status ProcessMetadataReq(PacketDecoder* decoder, Request* req) {
  PX_ASSIGN_OR_RETURN(MetadataReq r, decoder->ExtractMetadataReq());

  req->msg = ToString(r);
  return Status::OK();
}

Status ProcessReq(Packet* req_packet, Request* req) {
  req->timestamp_ns = req_packet->timestamp_ns;
  PacketDecoder decoder(*req_packet);
  // Extracts api_key, api_version, and correlation_id.
  PX_RETURN_IF_ERROR(decoder.ExtractReqHeader(req));

  // TODO(chengruizhe): Add support for more api keys.
  switch (req->api_key) {
    case APIKey::kProduce:
      return ProcessProduceReq(&decoder, req);
    case APIKey::kFetch:
      return ProcessFetchReq(&decoder, req);
    case APIKey::kJoinGroup:
      return ProcessJoinGroupReq(&decoder, req);
    case APIKey::kSyncGroup:
      return ProcessSyncGroupReq(&decoder, req);
    case APIKey::kMetadata:
      return ProcessMetadataReq(&decoder, req);
    default:
      VLOG(1) << absl::Substitute("Unparsed cmd $0", magic_enum::enum_name(req->api_key));
  }
  return Status::OK();
}

Status ProcessResp(Packet* resp_packet, Response* resp, APIKey api_key, int16_t api_version) {
  resp->timestamp_ns = resp_packet->timestamp_ns;
  PacketDecoder decoder(*resp_packet);
  decoder.SetAPIInfo(api_key, api_version);

  PX_RETURN_IF_ERROR(decoder.ExtractRespHeader(resp));

  switch (api_key) {
    case APIKey::kProduce:
      return ProcessProduceResp(&decoder, resp);
    case APIKey::kFetch:
      return ProcessFetchResp(&decoder, resp);
    case APIKey::kJoinGroup:
      return ProcessJoinGroupResp(&decoder, resp);
    case APIKey::kSyncGroup:
      return ProcessSyncGroupResp(&decoder, resp);
    // TODO(chengruizhe): Add support for more api keys.
    default:
      VLOG(1) << absl::Substitute("Unparsed cmd $0", magic_enum::enum_name(api_key));
  }

  return Status::OK();
}

StatusOr<Record> ProcessReqRespPair(Packet* req_packet, Packet* resp_packet) {
  CTX_ECHECK_LT(req_packet->timestamp_ns, resp_packet->timestamp_ns);

  Record r;
  PX_RETURN_IF_ERROR(ProcessReq(req_packet, &r.req));
  PX_RETURN_IF_ERROR(ProcessResp(resp_packet, &r.resp, r.req.api_key, r.req.api_version));
  return r;
}

// Kafka StitchFrames uses a hashmap to store a mapping of correlation_ids to resp_packets.
// For each req_packet, it looks for the corresponding resp_packet in the map.
// All the resp_packets, whether matched with a request or not, are popped off at the end of
// the function, where req_packets not matched remain in the deque.
// Note that this is different from the two for loop implementation used in other stitchers.
RecordsWithErrorCount<Record> StitchFrames(std::deque<Packet>* req_packets,
                                           std::deque<Packet>* resp_packets, State* state) {
  std::vector<Record> entries;
  int error_count = 0;

  // Maps correlation_id to resp packet.
  absl::flat_hash_map<int32_t, Packet*> correlation_id_map;
  for (auto& resp_packet : *resp_packets) {
    correlation_id_map[resp_packet.correlation_id] = &resp_packet;
  }

  for (auto& req_packet : *req_packets) {
    auto it = correlation_id_map.find(req_packet.correlation_id);
    if (it != correlation_id_map.end()) {
      StatusOr<Record> record_status = ProcessReqRespPair(&req_packet, it->second);
      if (record_status.ok()) {
        entries.push_back(record_status.ConsumeValueOrDie());
      } else {
        VLOG(1) << record_status.ToString();
        ++error_count;
      }
      // Mark the request as consumed, and clean-up when they reach the head of the queue.
      req_packet.consumed = true;
      // Remove resp_packet from map once it's been matched.
      correlation_id_map.erase(req_packet.correlation_id);
      // Remove this correlation_id from state.
      // TODO(chengruizhe): Add expiration time for correlation_ids in the state.
      state->seen_correlation_ids.erase(req_packet.correlation_id);
    }
  }

  // Resp packets left in the map don't have a matched request.
  for (const auto& [correlation_id, resp_packet] : correlation_id_map) {
    VLOG(1) << absl::Substitute("Did not find a request matching the response. Correlation ID = $0",
                                correlation_id);
    ++error_count;
  }

  // Clean-up consumed req_packets at the head.
  auto it = req_packets->begin();
  while (it != req_packets->end()) {
    if (!(*it).consumed) {
      break;
    }
    it++;
  }
  req_packets->erase(req_packets->begin(), it);

  resp_packets->clear();

  // TODO(chengruizhe): Remove req_packets that are too old.
  return {entries, error_count};
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
