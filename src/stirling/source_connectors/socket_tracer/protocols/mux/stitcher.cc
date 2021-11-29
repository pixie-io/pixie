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

#include "src/stirling/source_connectors/socket_tracer/protocols/mux/stitcher.h"

#include <set>
#include <string>
#include <utility>
#include <variant>

#include <absl/strings/str_replace.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

RecordsWithErrorCount<mux::Record> StitchFrames(std::deque<mux::Frame>* reqs,
                                                std::deque<mux::Frame>* resps) {
  std::vector<mux::Record> records;
  int error_count = 0;

  for (auto& req : *reqs) {
    for (auto resp_it = resps->begin(); resp_it != resps->end(); resp_it++) {
      auto& resp = *resp_it;

      if (resp.consumed) continue;

      Type req_type = static_cast<Type>(req.type);

      // Tlease messages do not have a response pair
      if (req_type == Type::kTlease) {
        req.consumed = true;
        records.push_back({std::move(req), {}});
        break;
      }
      if (req.timestamp_ns > resp.timestamp_ns) {
        resp.consumed = true;
        error_count++;
        VLOG(1) << absl::Substitute(
            "Did not find a request matching the response. Tag = $0 Type = $1", uint32_t(resp.tag),
            resp.type);
        continue;
      }

      StatusOr<Type> matching_resp_type = GetMatchingRespType(req_type);
      Type res_type = static_cast<Type>(resp.type);
      if (!matching_resp_type.ok() || res_type != matching_resp_type.ValueOrDie() ||
          resp.tag != req.tag) {
        continue;
      }

      req.consumed = true;
      resp.consumed = true;
      records.push_back({std::move(req), std::move(resp)});
      break;
    }
  }
  // TODO(ddelnano): Clean up stale requests once there is a mechanism to do so
  auto it = reqs->begin();
  while (it != reqs->end()) {
    if (!(*it).consumed) {
      break;
    }
    it++;
  }
  reqs->erase(reqs->begin(), it);
  resps->clear();

  return {records, error_count};
}

}  // namespace mux
}  // namespace protocols
}  // namespace stirling
}  // namespace px
