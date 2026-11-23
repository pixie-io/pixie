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
#include <string_view>

#include <absl/strings/str_cat.h>

namespace px {
namespace stirling {
namespace protocols {
namespace tls {
namespace testutils {

inline std::string TLSRecord(uint8_t handshake_type, std::string_view body) {
  using std::string_view_literals::operator""sv;
  std::string handshake =
      absl::StrCat(std::string(1, static_cast<char>(handshake_type)), std::string(1, '\0'),
                   std::string(1, static_cast<char>(body.size() >> 8)),
                   std::string(1, static_cast<char>(body.size() & 0xff)), body);
  return absl::StrCat("\x16\x03\x01"sv, std::string(1, static_cast<char>(handshake.size() >> 8)),
                      std::string(1, static_cast<char>(handshake.size() & 0xff)), handshake);
}

inline std::string ClientHello() {
  using std::string_view_literals::operator""sv;
  return TLSRecord(1, absl::StrCat("\x03\x03"sv, std::string(32, 'r'),
                                   "\x00\x00\x02\x13\x01\x01\x00\x00\x00"sv));
}

inline std::string ServerHello() {
  using std::string_view_literals::operator""sv;
  return TLSRecord(2, absl::StrCat("\x03\x03"sv, std::string(32, 'r'),
                                   "\x00\x13\x01\x00\x00\x0c"
                                   "\x00\x2b\x00\x02\x03\x04"
                                   "\x00\x33\x00\x02\x11\xec"sv));
}

}  // namespace testutils
}  // namespace tls
}  // namespace protocols
}  // namespace stirling
}  // namespace px
