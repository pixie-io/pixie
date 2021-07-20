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
StatusOr<std::basic_string<TCharType>> PacketDecoder::ExtractBytesCore(int32_t len) {
  PL_ASSIGN_OR_RETURN(std::basic_string_view<TCharType> tbuf,
                      binary_decoder_.ExtractString<TCharType>(len));
  return std::basic_string<TCharType>(tbuf);
}

template <uint8_t TMaxLength>
StatusOr<int64_t> PacketDecoder::ExtractUnsignedVarintCore() {
  constexpr uint8_t kFirstBitMask = 0x80;
  constexpr uint8_t kLastSevenBitMask = 0x7f;
  constexpr uint8_t kByteLength = 7;

  int64_t value = 0;
  for (int i = 0; i < TMaxLength; i += kByteLength) {
    PL_ASSIGN_OR_RETURN(uint64_t b, binary_decoder_.ExtractChar());
    if (!(b & kFirstBitMask)) {
      value |= (b << i);
      return value;
    }
    value |= ((b & kLastSevenBitMask) << i);
  }
  return error::Internal("Extract Varint Core failure.");
}

template <uint8_t TMaxLength>
StatusOr<int64_t> PacketDecoder::ExtractVarintCore() {
  PL_ASSIGN_OR_RETURN(int64_t value, ExtractUnsignedVarintCore<TMaxLength>());
  // Casting to uint64_t for logical right shift.
  return (static_cast<uint64_t>(value) >> 1) ^ (-(value & 1));
}

/*
 * Primitive Type Parsers
 */

StatusOr<int8_t> PacketDecoder::ExtractInt8() { return binary_decoder_.ExtractInt<int8_t>(); }

StatusOr<int16_t> PacketDecoder::ExtractInt16() { return binary_decoder_.ExtractInt<int16_t>(); }

StatusOr<int32_t> PacketDecoder::ExtractInt32() { return binary_decoder_.ExtractInt<int32_t>(); }

StatusOr<int64_t> PacketDecoder::ExtractInt64() { return binary_decoder_.ExtractInt<int64_t>(); }

StatusOr<int32_t> PacketDecoder::ExtractUnsignedVarint() {
  constexpr uint8_t kVarintMaxLength = 35;
  return ExtractUnsignedVarintCore<kVarintMaxLength>();
}

StatusOr<int32_t> PacketDecoder::ExtractVarint() {
  constexpr uint8_t kVarintMaxLength = 35;
  return ExtractVarintCore<kVarintMaxLength>();
}

StatusOr<int64_t> PacketDecoder::ExtractVarlong() {
  constexpr uint8_t kVarlongMaxLength = 70;
  return ExtractVarintCore<kVarlongMaxLength>();
}

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

StatusOr<std::string> PacketDecoder::ExtractCompactString() {
  PL_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
  // length N + 1 is encoded.
  len -= 1;
  if (len < 0) {
    return error::Internal("Compact String has negative length.");
  }
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractCompactNullableString() {
  PL_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
  // length N + 1 is encoded.
  len -= 1;
  if (len < -1) {
    return error::Internal("Compact Nullable String has negative length.");
  }
  if (len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

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

StatusOr<std::string> PacketDecoder::ExtractBytesZigZag() {
  PL_ASSIGN_OR_RETURN(int32_t len, ExtractVarint());
  if (len < -1) {
    return error::Internal("Not enough bytes in ExtractBytesZigZag.");
  }
  if (len == 0 || len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

/*
 * Header Parsers
 */

Status PacketDecoder::ExtractReqHeader(Request* req) {
  PL_ASSIGN_OR_RETURN(int16_t api_key, ExtractInt16());
  req->api_key = static_cast<APIKey>(api_key);

  PL_ASSIGN_OR_RETURN(req->api_version, ExtractInt16());
  this->set_api_version(req->api_version);

  // Extract correlation_id.
  PL_RETURN_IF_ERROR(ExtractInt32());
  PL_ASSIGN_OR_RETURN(req->client_id, ExtractNullableString());
  return Status::OK();
}

Status PacketDecoder::ExtractRespHeader(Response* /*resp*/) {
  // Extract correlation_id.
  PL_RETURN_IF_ERROR(ExtractInt32());
  return Status::OK();
}

/*
 * Message struct Parsers
 */

// TODO(chengruizhe): Add support for ProduceReq V9. It requires parsing compact strings, compact
//  records, and tag buffers.
// Documentation: https://kafka.apache.org/protocol.html#The_Messages_Produce
StatusOr<ProduceReq> PacketDecoder::ExtractProduceReq() {
  ProduceReq r;
  if (api_version_ >= 3) {
    PL_ASSIGN_OR_RETURN(r.transactional_id, ExtractNullableString());
  }

  PL_ASSIGN_OR_RETURN(r.acks, ExtractInt16());
  PL_ASSIGN_OR_RETURN(r.timeout_ms, ExtractInt32());
  PL_ASSIGN_OR_RETURN(r.num_topics, ExtractInt32());

  // TODO(chengruizhe): Add parsing of TopicData, and its downstream structs.
  return r;
}

StatusOr<ProduceResp> PacketDecoder::ExtractProduceResp() {
  ProduceResp r;

  PL_ASSIGN_OR_RETURN(r.num_responses, ExtractInt32());

  // TODO(chengruizhe): Add parsing of the responses and partitions.
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
