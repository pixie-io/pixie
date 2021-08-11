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

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <vector>

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

struct FetchReqPartition {
  int32_t index = 0;
  int32_t current_leader_epoch = -1;
  int64_t fetch_offset = 0;
  int32_t last_fetched_epoch = -1;
  int64_t log_start_offset = -1;
  int32_t partition_max_bytes = 0;
};

struct FetchReqTopic {
  std::string name;
  std::vector<FetchReqPartition> partitions;
};

struct FetchForgottenTopicsData {
  std::string name;
  std::vector<int32_t> partition_indices;
};

// https://github.com/apache/kafka/blob/trunk/clients/src/main/resources/common/message/FetchRequest.json
struct FetchReq {
  int32_t replica_id = 0;
  int32_t session_id = 0;
  int32_t session_epoch = -1;
  std::vector<FetchReqTopic> topics;
  std::vector<FetchForgottenTopicsData> forgotten_topics;
  std::string rack_id;
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
