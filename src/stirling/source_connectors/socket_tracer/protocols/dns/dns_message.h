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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

struct DNSQuestion {
  std::string name;
  uint16_t qtype = 0;
};

struct DNSAnswer {
  std::string name;
  uint16_t type = 0;
  std::string addr;
  std::string target;
};

constexpr uint16_t kDNSTypeA = 1;
constexpr uint16_t kDNSTypeCNAME = 5;
constexpr uint16_t kDNSTypePTR = 12;
constexpr uint16_t kDNSTypeAAAA = 28;

struct DNSMessageInfo {
  size_t length = 0;
  uint16_t txid = 0;
  uint16_t flags = 0;
  std::vector<DNSQuestion> questions;
  std::vector<DNSAnswer> answers;
};

bool WalkDNSMessage(std::string_view buf, DNSMessageInfo* info);

std::string DNSTypeName(uint16_t type);

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
