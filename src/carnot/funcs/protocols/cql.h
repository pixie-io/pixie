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
namespace carnot {
namespace funcs {
namespace protocols {
namespace cql {

inline std::string RequestOpcodeToName(int req_op) {
  switch (req_op) {
    case 0x00:
      return "Error";
    case 0x01:
      return "Startup";
    case 0x02:
      return "Ready";
    case 0x03:
      return "Authenticate";
    case 0x05:
      return "Options";
    case 0x06:
      return "Supported";
    case 0x07:
      return "Query";
    case 0x08:
      return "Result";
    case 0x09:
      return "Prepare";
    case 0x0a:
      return "Execute";
    case 0x0b:
      return "Register";
    case 0x0c:
      return "Event";
    case 0x0d:
      return "Batch";
    case 0x0e:
      return "AuthChallenge";
    case 0x0f:
      return "AuthResponse";
    case 0x10:
      return "AuthSuccess";
    default:
      return std::to_string(req_op);
  }
}

}  // namespace cql

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
