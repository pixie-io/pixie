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

#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/message_set.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

struct ProduceReqPartition {
  int32_t index = 0;
  MessageSet message_set;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("index", index);
    builder->WriteKVRecursive("message_set", message_set);
  }
};

struct ProduceReqTopic {
  std::string name;
  std::vector<ProduceReqPartition> partitions;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("name", name);
    builder->WriteKVArrayRecursive<ProduceReqPartition>("partitions", partitions);
  }
};

// Produce Request Message (opcode = 0).
struct ProduceReq {
  std::string transactional_id;
  int16_t acks = 0;
  int32_t timeout_ms = 0;
  std::vector<ProduceReqTopic> topics;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("transactional_id", transactional_id);
    builder->WriteKV("acks", acks);
    builder->WriteKV("timeout_ms", timeout_ms);
    builder->WriteKVArrayRecursive<ProduceReqTopic>("topics", topics);
  }
};

struct RecordError {
  int32_t batch_index = 0;
  std::string error_message;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("batch_index", batch_index);
    builder->WriteKV("error_message", error_message);
  }
};

struct ProduceRespPartition {
  int32_t index = 0;
  int16_t error_code = 0;
  int64_t base_offset = 0;
  int64_t log_append_time_ms = -1;
  int64_t log_start_offset = 0;
  std::vector<RecordError> record_errors;
  std::string error_message;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("index", index);
    builder->WriteKV("error_code", error_code);
    builder->WriteKV("base_offset", base_offset);
    builder->WriteKV("log_append_time_ms", log_append_time_ms);
    builder->WriteKV("log_start_offset", log_start_offset);
    builder->WriteKVArrayRecursive<RecordError>("record_errors", record_errors);
    builder->WriteKV("error_message", error_message);
  }
};

struct ProduceRespTopic {
  std::string name;
  std::vector<ProduceRespPartition> partitions;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("name", name);
    builder->WriteKVArrayRecursive<ProduceRespPartition>("partitions", partitions);
  }
};

struct ProduceResp {
  std::vector<ProduceRespTopic> topics;
  int32_t throttle_time_ms = 0;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKVArrayRecursive<ProduceRespTopic>("topics", topics);
    builder->WriteKV("throttle_time_ms", throttle_time_ms);
  }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
