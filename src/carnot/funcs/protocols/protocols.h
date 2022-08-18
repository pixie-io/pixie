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

#include "src/shared/protocols/protocols.h"

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {

inline std::string ProtocolName(int protocol) {
  switch (static_cast<px::protocols::Protocol>(protocol)) {
    case px::protocols::Protocol::kUnknown:
      return "Unknown";
    case px::protocols::Protocol::kHTTP:
      return "HTTP";
    case px::protocols::Protocol::kHTTP2:
      return "HTTP2";
    case px::protocols::Protocol::kMySQL:
      return "MySQL";
    case px::protocols::Protocol::kCQL:
      return "CQL";
    case px::protocols::Protocol::kPGSQL:
      return "PGSQL";
    case px::protocols::Protocol::kDNS:
      return "DNS";
    case px::protocols::Protocol::kRedis:
      return "Redis";
    case px::protocols::Protocol::kNATS:
      return "NATS";
    case px::protocols::Protocol::kMongo:
      return "Mongo";
    case px::protocols::Protocol::kKafka:
      return "Kafka";
    case px::protocols::Protocol::kAMQP:
      return "AMQP";
    default:
      return absl::StrCat("Invalid (", protocol, ")");
  }
}

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
