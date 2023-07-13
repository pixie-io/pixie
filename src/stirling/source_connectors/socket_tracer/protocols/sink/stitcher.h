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

#include <deque>
#include <limits>
#include <utility>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/timestamp_stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/sink/types.h"

namespace px {
namespace stirling {
namespace protocols {

template <>
inline RecordsWithErrorCount<sink::Record> StitchFrames(std::deque<sink::Message>* req_messages,
                                                        std::deque<sink::Message>* resp_messages,
                                                        NoState* /* state */) {
  std::vector<sink::Record> records;
  sink::Record record;
  records.push_back(record);

  req_messages->erase(req_messages->begin(), req_messages->end());
  resp_messages->erase(resp_messages->begin(), resp_messages->end());

  return {std::move(records), 0};
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
