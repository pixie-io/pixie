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
                                                  std::deque<mux::Frame>* resps,
                                                  NoState* state) {
    PL_UNUSED(state);
    std::vector<mux::Record> records;
    int error_count = 0;

    auto req_iter = reqs->begin();
    auto resp_iter = resps->begin();
    while (req_iter != reqs->end() && resp_iter != resps->end()) {

        if (Type(req_iter->type) == Type::Tlease) {

            records.push_back({std::move(*req_iter), {}});
            ++req_iter;
            continue;
        }
        if (req_iter->timestamp_ns > resp_iter->timestamp_ns) {

            records.push_back({{}, std::move(*resp_iter)});
            ++resp_iter;
            continue;
        }

        Type matching_resp_type = GetMatchingRespType(Type(req_iter->type));
        if (
            resp_iter->type != static_cast<int8_t>(matching_resp_type) ||
            resp_iter->tag != req_iter->tag
        ) {
            ++resp_iter;
            continue;
        }

        records.push_back({std::move(*req_iter), std::move(*resp_iter)});
        ++req_iter;
        ++resp_iter;
    }

    while (req_iter != reqs->end()) {
        records.push_back({std::move(*req_iter), {}});

        ++req_iter;
        error_count++;
    }

    /* while (resp_iter != resps->end()) { */
    /*     records.push_back({{}, std::move(*resp_iter)}); */

    /*     if (Type(resp_iter->type) == Type::Rerr) { */

    /*     } */
    /*     ++req_iter; */
    /*     records.error_count++; */
    /* } */

    reqs->erase(reqs->begin(), req_iter);
    resps->erase(resps->begin(), resp_iter);

    return {records, error_count};
}

}  // namespace mux
}  // namespace protocols
}  // namespace stirling
}  // namespace px
