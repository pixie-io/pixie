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

#pragma once

#include <string>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/frame_body_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

// Equality operators for types. C++20 would make this a lot easier:
// https://en.cppreference.com/w/cpp/language/default_comparisons
bool operator==(const Option& a, const Option& b);
bool operator==(const NameValuePair& a, const NameValuePair& b);
bool operator==(const QueryParameters& a, const QueryParameters& b);
bool operator==(const ColSpec& a, const ColSpec& b);
bool operator==(const ResultMetadata& a, const ResultMetadata& b);
bool operator==(const SchemaChange& a, const SchemaChange& b);
bool operator==(const QueryReq& a, const QueryReq& b);
bool operator==(const ResultVoidResp&, const ResultVoidResp&);
bool operator==(const ResultRowsResp& a, const ResultRowsResp& b);
bool operator==(const ResultSetKeyspaceResp& a, const ResultSetKeyspaceResp& b);
bool operator==(const ResultPreparedResp& a, const ResultPreparedResp& b);
bool operator==(const ResultSchemaChangeResp& a, const ResultSchemaChangeResp& b);
bool operator==(const ResultResp& a, const ResultResp& b);

namespace testutils {

template <typename TOpType>
inline std::string CreateCQLHeader(TOpType op, uint16_t stream, size_t length) {
  static_assert(std::is_same_v<TOpType, cass::ReqOp> || std::is_same_v<TOpType, cass::RespOp>);

  std::string hdr;
  hdr.resize(9);
  hdr[0] = 0x04;           // direction + version
  hdr[1] = 0x00;           // flags
  hdr[2] = (stream >> 8);  // stream
  hdr[3] = (stream >> 0);
  hdr[4] = static_cast<uint8_t>(op);  // opcode
  hdr[5] = length >> 24;              // length
  hdr[6] = length >> 16;
  hdr[7] = length >> 8;
  hdr[8] = length >> 0;

  if (std::is_same_v<TOpType, cass::RespOp>) {
    hdr[0] = hdr[0] | kDirectionMask;
  }

  return hdr;
}

template <typename TOpType>
inline std::string CreateCQLEvent(TOpType op, std::string_view body, uint16_t stream) {
  std::string hdr = CreateCQLHeader(op, stream, body.length());
  return absl::StrCat(hdr, body);
}

template <typename TOpType, size_t N>
inline std::string CreateCQLEvent(TOpType op, const uint8_t (&a)[N], uint16_t stream) {
  std::string_view body = CreateCharArrayView<char>(a);
  return CreateCQLEvent(op, body, stream);
}

template <typename TOpType>
inline std::string CreateCQLEmptyEvent(TOpType op, uint16_t stream) {
  std::string_view body = "";
  std::string hdr = CreateCQLHeader(op, stream, body.length());
  return absl::StrCat(hdr, body);
}

// Required because zero length C-arrays are not allowed in C++, so can't use the version above.
inline Frame CreateFrame(uint16_t stream, Opcode opcode, uint64_t timestamp_ns) {
  // Should be either a request or response opcode.
  CHECK(IsReqOpcode(opcode) != IsRespOpcode(opcode));

  Frame f;
  f.hdr.opcode = opcode;
  f.hdr.stream = stream;
  f.hdr.flags = 0;
  f.hdr.version = 0x04;
  f.hdr.flags = 0;
  f.hdr.length = 0;
  f.msg = "";
  f.timestamp_ns = timestamp_ns;
  return f;
}

inline Frame CreateFrame(uint16_t stream, Opcode opcode, std::string_view body,
                         uint64_t timestamp_ns) {
  Frame f = CreateFrame(stream, opcode, timestamp_ns);

  f.hdr.length = body.length();
  f.msg = body;

  return f;
}

// This version populates a body.
template <size_t N>
inline Frame CreateFrame(uint16_t stream, Opcode opcode, const uint8_t (&msg)[N],
                         uint64_t timestamp_ns) {
  std::string_view msg_view = CreateCharArrayView<char>(msg);
  return CreateFrame(stream, opcode, msg_view, timestamp_ns);
}

// Data Generation utilities.
std::string ResultRespToByteString(const ResultResp& result_resp);
std::string QueryReqToByteString(const QueryReq& req);
std::string QueryReqToEvent(const QueryReq& req, uint16_t stream);
std::string RowToByteString(const std::vector<std::string_view>& cols);

}  // namespace testutils
}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
