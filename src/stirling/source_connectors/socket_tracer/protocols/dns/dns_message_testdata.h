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

#include <cstdint>
#include <string>
#include <string_view>

namespace px {
namespace stirling {
namespace protocols {
namespace dns {
namespace testdata {

constexpr std::string_view kPTRQueryHex =
    "123401000001000000000000023132013002343202313007696e2d61646472046172706100000c0001";

constexpr std::string_view kPTRRespHex =
    "123485000001000100000000023132013002343202313007696e2d61646472046172706100000c0001023132"
    "013002343202313007696e2d61646472046172706100000c00010000000500330a31302d34322d302d313208"
    "6b7562652d646e730b6b7562652d73797374656d0373766307636c7573746572056c6f63616c00";

constexpr std::string_view kAQueryHex =
    "234501000001000000000000086b7562652d646e730b6b7562652d73797374656d0373766307636c75737465"
    "72056c6f63616c0000010001";

constexpr std::string_view kARespHex =
    "234585000001000100000000086b7562652d646e730b6b7562652d73797374656d0373766307636c75737465"
    "72056c6f63616c0000010001086b7562652d646e730b6b7562652d73797374656d0373766307636c75737465"
    "72056c6f63616c00000100010000000500040a2b000a";

constexpr std::string_view kCompressedRespHex =
    "c6fa8180000100010000000114696e74656c6c696a2d6578706572696d656e74730761707073706f7403636f"
    "6d0000010001c00c00010001000001240004d83ac2b40000290200000000000000";

inline std::string U16(uint16_t v) {
  return std::string{static_cast<char>(v >> 8), static_cast<char>(v & 0xff)};
}

inline std::string Name(std::string_view dotted) {
  std::string out;
  size_t start = 0;
  while (start < dotted.size()) {
    size_t dot = dotted.find('.', start);
    if (dot == std::string_view::npos) dot = dotted.size();
    out.push_back(static_cast<char>(dot - start));
    out.append(dotted.substr(start, dot - start));
    start = dot + 1;
  }
  out.push_back('\0');
  return out;
}

inline std::string Header(uint16_t txid, uint16_t flags, uint16_t qd, uint16_t an, uint16_t ns = 0,
                          uint16_t ar = 0) {
  return U16(txid) + U16(flags) + U16(qd) + U16(an) + U16(ns) + U16(ar);
}

inline std::string RR(std::string_view owner_wire, uint16_t type, std::string_view rdata) {
  return std::string(owner_wire) + U16(type) + U16(1) + U16(0) + U16(5) +
         U16(static_cast<uint16_t>(rdata.size())) + std::string(rdata);
}

inline std::string Bytes(std::string_view hex) {
  std::string out;
  out.reserve(hex.size() / 2);
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
  };
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    out.push_back(static_cast<char>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
  }
  return out;
}

}  // namespace testdata
}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
