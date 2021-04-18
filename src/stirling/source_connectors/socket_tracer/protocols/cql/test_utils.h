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

namespace px {
namespace stirling {
namespace protocols {
namespace cass {
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

template <typename TOpType, size_t N>
inline std::string CreateCQLEvent(TOpType op, const uint8_t (&a)[N], uint16_t stream) {
  std::string_view body = CreateCharArrayView<char>(a);
  std::string hdr = CreateCQLHeader(op, stream, body.length());
  return absl::StrCat(hdr, body);
}

template <typename TOpType>
inline std::string CreateCQLEmptyEvent(TOpType op, uint16_t stream) {
  std::string_view body = "";
  std::string hdr = CreateCQLHeader(op, stream, body.length());
  return absl::StrCat(hdr, body);
}

static constexpr int kCQLReqOpIdx = kCQLTable.ColIndex("req_op");
static constexpr int kCQLReqBodyIdx = kCQLTable.ColIndex("req_body");
static constexpr int kCQLRespOpIdx = kCQLTable.ColIndex("resp_op");
static constexpr int kCQLRespBodyIdx = kCQLTable.ColIndex("resp_body");
static constexpr int kCQLLatencyIdx = kCQLTable.ColIndex("latency");

}  // namespace testutils
}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
