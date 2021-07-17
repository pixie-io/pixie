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

#include <deque>
#include <string>
#include <unordered_map>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/packet_decoder.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

Status ProcessProduceReq(PacketDecoder* decoder, Request* req) {
  PL_ASSIGN_OR_RETURN(ProduceReq r, decoder->ExtractProduceReq());

  req->msg = r.ToJSONString();
  return Status::OK();
}

Status ProcessProduceResp(PacketDecoder* decoder, Response* resp) {
  PL_ASSIGN_OR_RETURN(ProduceResp r, decoder->ExtractProduceResp());

  resp->msg = r.ToJSONString();
  return Status::OK();
}

Status ProcessReq(Packet* req_packet, Request* req) {
  PacketDecoder decoder(*req_packet);

  // Extracts api_key, api_version, and correlation_id.
  PL_RETURN_IF_ERROR(decoder.ExtractReqHeader(req));

  switch (req->api_key) {
    case APIKey::kProduce:
      return ProcessProduceReq(&decoder, req);
    // TODO(chengruizhe): Add support for more api keys.
    default:
      return error::Internal("Unhandled cmd $0", magic_enum::enum_name(req->api_key));
  }
  return Status::OK();
}

Status ProcessResp(Packet* resp_packet, Response* resp, APIKey api_key, int16_t api_version) {
  PacketDecoder decoder(*resp_packet);
  decoder.set_api_version(api_version);

  PL_RETURN_IF_ERROR(decoder.ExtractRespHeader(resp));

  switch (api_key) {
    case APIKey::kProduce:
      return ProcessProduceResp(&decoder, resp);
    // TODO(chengruizhe): Add support for more api keys.
    default:
      return error::Internal("Unhandled cmd $0", magic_enum::enum_name(api_key));
  }

  return Status::OK();
}

StatusOr<Record> ProcessReqRespPair(Packet* req_packet, Packet* resp_packet) {
  ECHECK_LT(req_packet->timestamp_ns, resp_packet->timestamp_ns);

  Record r;
  PL_RETURN_IF_ERROR(ProcessReq(req_packet, &r.req));
  PL_RETURN_IF_ERROR(ProcessResp(resp_packet, &r.resp, r.req.api_key, r.req.api_version));

  return r;
}

// Kafka StitchFrames uses a hashmap to store a mapping of correlation_ids to resp_packets.
// For each req_packet, it looks for the corresponding resp_packet in the map.
// All the resp_packets, whether matched with a request or not, are popped off at the end of
// the function, where req_packets not matched remain in the deque.
// Note that this is different from the two for loop implementation used in other stitchers.
RecordsWithErrorCount<Record> StitchFrames(std::deque<Packet>* req_packets,
                                           std::deque<Packet>* resp_packets) {
  std::vector<Record> entries;
  int error_count = 0;

  // Maps correlation_id to resp packet.
  std::unordered_map<int32_t, Packet*> correlation_id_map;
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
      break;
    }
  }

  // Resp packets left in the map don't have a matched request.
  for (const auto& [correlation_id, resp_packet] : correlation_id_map) {
    VLOG(1) << absl::Substitute("Did not find a request matching the response. Correlation ID = $0",
                                correlation_id);
    ++error_count;
  }

  // Clean-up consumed req_packets at the head.
  for (const auto& req_packet : *req_packets) {
    if (!req_packet.consumed) {
      break;
    }
    req_packets->pop_front();
  }

  resp_packets->clear();

  // TODO(chengruizhe): Remove req_packets that are too old.
  return {entries, error_count};
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
