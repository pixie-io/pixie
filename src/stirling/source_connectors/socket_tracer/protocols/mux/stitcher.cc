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

#include <string>
#include <utility>
#include <variant>
#include <set>

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
        auto req_consumed = false;
        for (auto& res : *resps) {
            Type req_type = static_cast<Type>(req.type);
            if (req_type == Type::kTlease) {

                records.push_back({req, {}});
                req_consumed = true;
                break;
            }
            if (req.timestamp_ns > res.timestamp_ns) {

                records.push_back({{}, res});
                resps->pop_front();
                error_count++;
                continue;
            }

            std::optional<Type> matching_resp_type = GetMatchingRespType(req_type);
            Type res_type = static_cast<Type>(res.type);
            if (
                !matching_resp_type.has_value() ||
                res_type != matching_resp_type.value() ||
                res.tag != req.tag
            ) {
                continue;
            }

            records.push_back({req, res});
            resps->pop_front();
            req_consumed = true;
            break;
        }

        if (! req_consumed) {
            records.push_back({req, {}});

            error_count++;
        }
    }
    reqs->erase(reqs->begin(), reqs->end());
    resps->erase(resps->begin(), resps->end());

    return {records, error_count};
}

}  // namespace mux
}  // namespace protocols
}  // namespace stirling
}  // namespace px
