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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/produce.h"

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

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("index", index);
    builder->WriteKV("current_leader_epoch", current_leader_epoch);
    builder->WriteKV("fetch_offset", fetch_offset);
    builder->WriteKV("last_fetched_epoch", last_fetched_epoch);
    builder->WriteKV("log_start_offset", log_start_offset);
    builder->WriteKV("partition_max_bytes", partition_max_bytes);
  }
};

struct FetchReqTopic {
  std::string name;
  std::vector<FetchReqPartition> partitions;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("name", name);
    builder->WriteKVArrayRecursive<FetchReqPartition>("partitions", partitions);
  }
};

struct FetchForgottenTopicsData {
  std::string name;
  std::vector<int32_t> partition_indices;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("name", name);
    builder->WriteKV("partitions_indices", partition_indices);
  }
};

// https://github.com/apache/kafka/blob/trunk/clients/src/main/resources/common/message/FetchRequest.json
struct FetchReq {
  int32_t replica_id = 0;
  int32_t session_id = 0;
  int32_t session_epoch = -1;
  std::vector<FetchReqTopic> topics;
  std::vector<FetchForgottenTopicsData> forgotten_topics;
  std::string rack_id;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("replica_id", replica_id);
    builder->WriteKV("session_id", session_id);
    builder->WriteKV("session_epoch", session_epoch);
    builder->WriteKVArrayRecursive<FetchReqTopic>("topics", topics);
    builder->WriteKVArrayRecursive<FetchForgottenTopicsData>("forgotten_topics", forgotten_topics);
    builder->WriteKV("rack_id", rack_id);
  }
};

struct FetchRespAbortedTransaction {
  int64_t producer_id;
  int64_t first_offset;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("producer_id", producer_id);
    builder->WriteKV("first_offset", first_offset);
  }
};

struct FetchRespPartition {
  int32_t index = 0;
  int16_t error_code = 0;
  int64_t high_watermark = 0;
  int64_t last_stable_offset = -1;
  int64_t log_start_offset = -1;
  std::vector<FetchRespAbortedTransaction> aborted_transactions;
  int32_t preferred_read_replica = -1;
  MessageSet message_set;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("index", index);
    builder->WriteKV("error_code", error_code);
    builder->WriteKV("high_watermark", high_watermark);
    builder->WriteKV("last_stable_offset", last_stable_offset);
    builder->WriteKV("log_start_offset", log_start_offset);
    builder->WriteKVArrayRecursive<FetchRespAbortedTransaction>("aborted_transactions",
                                                                aborted_transactions);
    builder->WriteKV("preferred_read_replica", preferred_read_replica);
    builder->WriteKVRecursive("message_set", message_set);
  }
};

struct FetchRespTopic {
  std::string name;
  std::vector<FetchRespPartition> partitions;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("name", name);
    builder->WriteKVArrayRecursive<FetchRespPartition>("partitions", partitions);
  }
};

struct FetchResp {
  int32_t throttle_time_ms;
  int16_t error_code = 0;
  int32_t session_id = 0;
  std::vector<FetchRespTopic> topics;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("throttle_time_ms", throttle_time_ms);
    builder->WriteKV("error_code", error_code);
    builder->WriteKV("session_id", session_id);
    builder->WriteKVArrayRecursive<FetchRespTopic>("topics", topics);
  }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
