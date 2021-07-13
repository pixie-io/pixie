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

#include "src/stirling/source_connectors/socket_tracer/protocols/nats/parse.h"

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/nats/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace nats {

size_t FindMessageBoundary(std::string_view buf, size_t start_pos) {
  // Based on https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md.
  const std::vector<std::string_view> kMessageTypes = {"INFO", "CONNECT", "PUB",  "SUB", "UNSUB",
                                                       "MSG",  "PING",    "PONG", "+OK", "-ERR"};
  constexpr size_t kMinMsgSize = 3;
  for (size_t i = start_pos; i < buf.size() - kMinMsgSize; ++i) {
    for (auto msg_type : kMessageTypes) {
      if (absl::StartsWith(buf.substr(i), msg_type)) {
        return i;
      }
    }
  }
  return std::string_view::npos;
}

}  // namespace nats
}  // namespace protocols
}  // namespace stirling
}  // namespace px
