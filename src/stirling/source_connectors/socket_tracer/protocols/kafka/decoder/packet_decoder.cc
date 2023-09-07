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
#include <string>
#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

// TODO(chengruizhe): Many of the methods here are shareable with other protocols such as CQL.

template <typename TCharType>
StatusOr<std::basic_string<TCharType>> PacketDecoder::ExtractBytesCore(int32_t len) {
  PX_ASSIGN_OR_RETURN(std::basic_string_view<TCharType> tbuf,
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
    PX_ASSIGN_OR_RETURN(uint64_t b, binary_decoder_.ExtractChar());
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
  PX_ASSIGN_OR_RETURN(int64_t value, ExtractUnsignedVarintCore<TMaxLength>());
  // Casting to uint64_t for logical right shift.
  return (static_cast<uint64_t>(value) >> 1) ^ (-(value & 1));
}

/*
 * Primitive Type Parsers
 */
StatusOr<bool> PacketDecoder::ExtractBool() { return binary_decoder_.ExtractBEInt<bool>(); }

StatusOr<int8_t> PacketDecoder::ExtractInt8() { return binary_decoder_.ExtractBEInt<int8_t>(); }

StatusOr<int16_t> PacketDecoder::ExtractInt16() { return binary_decoder_.ExtractBEInt<int16_t>(); }

StatusOr<int32_t> PacketDecoder::ExtractInt32() { return binary_decoder_.ExtractBEInt<int32_t>(); }

StatusOr<int64_t> PacketDecoder::ExtractInt64() { return binary_decoder_.ExtractBEInt<int64_t>(); }

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

StatusOr<std::string> PacketDecoder::ExtractRegularString() {
  PX_ASSIGN_OR_RETURN(int16_t len, ExtractInt16());
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractRegularNullableString() {
  PX_ASSIGN_OR_RETURN(int16_t len, ExtractInt16());
  if (len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractCompactString() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
  // length N + 1 is encoded.
  len -= 1;
  if (len < 0) {
    return error::Internal("Compact String has negative length.");
  }
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractCompactNullableString() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
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

StatusOr<std::string> PacketDecoder::ExtractString() {
  if (is_flexible_) {
    return ExtractCompactString();
  }
  return ExtractRegularString();
}

StatusOr<std::string> PacketDecoder::ExtractNullableString() {
  if (is_flexible_) {
    return ExtractCompactNullableString();
  }
  return ExtractRegularNullableString();
}

StatusOr<std::string> PacketDecoder::ExtractRegularBytes() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractInt16());
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractRegularNullableBytes() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractInt16());
  if (len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractCompactBytes() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
  // length N + 1 is encoded.
  len -= 1;
  if (len < 0) {
    return error::Internal("Compact Bytes has negative length.");
  }
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractCompactNullableBytes() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
  // length N + 1 is encoded.
  len -= 1;
  if (len < -1) {
    return error::Internal("Compact Nullable Bytes has negative length.");
  }
  if (len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

StatusOr<std::string> PacketDecoder::ExtractBytes() {
  if (is_flexible_) {
    return ExtractCompactBytes();
  }
  return ExtractRegularBytes();
}

StatusOr<std::string> PacketDecoder::ExtractNullableBytes() {
  if (is_flexible_) {
    return ExtractCompactNullableBytes();
  }
  return ExtractRegularNullableBytes();
}

StatusOr<std::string> PacketDecoder::ExtractBytesZigZag() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractVarint());
  if (len < -1) {
    return error::Internal("Not enough bytes in ExtractBytesZigZag.");
  }
  if (len == 0 || len == -1) {
    return std::string();
  }
  return ExtractBytesCore<char>(len);
}

Status PacketDecoder::ExtractTagSection() {
  if (!is_flexible_) {
    return Status::OK();
  }

  PX_ASSIGN_OR_RETURN(int32_t num_fields, ExtractUnsignedVarint());
  for (int i = 0; i < num_fields; ++i) {
    PX_RETURN_IF_ERROR(ExtractTaggedField());
  }
  return Status::OK();
}

Status PacketDecoder::ExtractTaggedField() {
  PX_RETURN_IF_ERROR(/* tag */ ExtractUnsignedVarint());
  PX_ASSIGN_OR_RETURN(int32_t length, ExtractUnsignedVarint());
  PX_RETURN_IF_ERROR(/* data */ ExtractBytesCore<char>(length));
  return Status::OK();
}

/*
 * Header Parsers
 */

Status PacketDecoder::ExtractReqHeader(Request* req) {
  PX_ASSIGN_OR_RETURN(int16_t api_key, ExtractInt16());
  req->api_key = static_cast<APIKey>(api_key);

  PX_ASSIGN_OR_RETURN(req->api_version, ExtractInt16());
  SetAPIInfo(req->api_key, req->api_version);

  PX_RETURN_IF_ERROR(/* correlation_id */ ExtractInt32());
  // client_id is not compact, regardless of flexibility.
  PX_ASSIGN_OR_RETURN(req->client_id, ExtractRegularNullableString());

  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return Status::OK();
}

Status PacketDecoder::ExtractRespHeader(Response* /*resp*/) {
  PX_RETURN_IF_ERROR(/* correlation_id */ ExtractInt32());
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());

  return Status::OK();
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
