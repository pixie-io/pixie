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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/packet_decoder.h"
#include <string>
#include "src/common/base/byte_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

// TODO(chengruizhe): Many of the methods here are shareable with other protocols such as CQL.

template <typename TCharType>
StatusOr<std::basic_string<TCharType>> PacketDecoder::ExtractBytesCore(int16_t len) {
  PL_ASSIGN_OR_RETURN(std::basic_string_view<TCharType> tbuf,
                      binary_decoder_.ExtractString<TCharType>(len));
  return std::basic_string<TCharType>(tbuf);
}

/*
 * Primitive Type Parsers
 */

StatusOr<int8_t> PacketDecoder::ExtractInt8() { return binary_decoder_.ExtractInt<int8_t>(); }

StatusOr<int16_t> PacketDecoder::ExtractInt16() { return binary_decoder_.ExtractInt<int16_t>(); }

StatusOr<int32_t> PacketDecoder::ExtractInt32() { return binary_decoder_.ExtractInt<int32_t>(); }

StatusOr<int64_t> PacketDecoder::ExtractInt64() { return binary_decoder_.ExtractInt<int64_t>(); }

StatusOr<std::string> PacketDecoder::ExtractString() {
  PL_ASSIGN_OR_RETURN(int16_t len, ExtractInt16());
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractNullableString() {
  PL_ASSIGN_OR_RETURN(int16_t len, ExtractInt16());
  if (len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

/*
 * Message Struct Parsers
 */

Status ParseReqHeader(PacketDecoder* decoder, Request* req) {
  PL_ASSIGN_OR_RETURN(int16_t api_key, decoder->ExtractInt16());
  req->api_key = static_cast<APIKey>(api_key);
  PL_ASSIGN_OR_RETURN(req->api_version, decoder->ExtractInt16());
  // Extract correlation_id.
  PL_RETURN_IF_ERROR(decoder->ExtractInt32());
  PL_ASSIGN_OR_RETURN(req->client_id, decoder->ExtractNullableString());
  return Status::OK();
}

Status ParseRespHeader(PacketDecoder* decoder, Response* /*resp*/) {
  // Extract correlation_id.
  PL_RETURN_IF_ERROR(decoder->ExtractInt32());
  return Status::OK();
}

// TODO(chengruizhe): Add support for ProduceReq V9. It requires parsing compact strings, compact
//  records, and tag buffers.
// Documentation: https://kafka.apache.org/protocol.html#The_Messages_Produce
StatusOr<ProduceReq> ParseProduceReq(PacketDecoder* decoder, int16_t api_version) {
  ProduceReq r;
  if (api_version >= 3) {
    PL_ASSIGN_OR_RETURN(r.transactional_id, decoder->ExtractNullableString());
  }

  PL_ASSIGN_OR_RETURN(r.acks, decoder->ExtractInt16());
  PL_ASSIGN_OR_RETURN(r.timeout_ms, decoder->ExtractInt32());
  PL_ASSIGN_OR_RETURN(r.num_topics, decoder->ExtractInt32());

  // TODO(chengruizhe): Add parsing of TopicData, and its downstream structs.
  return r;
}

StatusOr<ProduceResp> ParseProduceResp(PacketDecoder* decoder, int16_t api_version) {
  ProduceResp r;

  PL_UNUSED(api_version);
  PL_ASSIGN_OR_RETURN(r.num_responses, decoder->ExtractInt32());

  // TODO(chengruizhe): Add parsing of the responses and partitions.
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
