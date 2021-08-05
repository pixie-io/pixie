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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/message_set.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

struct ProduceReqPartition {
  int32_t index = 0;
  RecordBatch record_batch;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("index");
    writer->Int(index);
    writer->Key("record_batch");
    record_batch.ToJSON(writer);
    writer->EndObject();
  }
};

struct ProduceReqTopic {
  std::string name;
  std::vector<ProduceReqPartition> partitions;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("name");
    writer->String(name.c_str());
    writer->Key("partitions");
    writer->StartArray();
    for (const auto& r : partitions) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

// Produce Request Message (opcode = 0).
struct ProduceReq {
  std::string transactional_id;
  int16_t acks = 0;
  int32_t timeout_ms = 0;
  std::vector<ProduceReqTopic> topics;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("transactional_id");
    writer->String(transactional_id.c_str());
    writer->Key("acks");
    writer->Int(acks);
    writer->Key("timeout_ms");
    writer->Int(timeout_ms);
    writer->Key("topics");
    writer->StartArray();
    for (const auto& r : topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

struct RecordError {
  int32_t batch_index;
  std::string error_message;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("batch_index");
    writer->Int(batch_index);
    writer->Key("error_message");
    writer->String(error_message.c_str());
    writer->EndObject();
  }
};

struct ProduceRespPartition {
  int32_t index;
  int16_t error_code;
  std::vector<RecordError> record_errors;
  std::string error_message;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("index");
    writer->Int(index);
    writer->Key("error_code");
    writer->String(magic_enum::enum_name(static_cast<ErrorCode>(error_code)).data());
    writer->Key("record_errors");
    writer->StartArray();
    for (const auto& r : record_errors) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("error_message");
    writer->String(error_message.c_str());
    writer->EndObject();
  }
};

struct ProduceRespTopic {
  std::string name;
  std::vector<ProduceRespPartition> partitions;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("name");
    writer->String(name.c_str());
    writer->Key("partitions");
    writer->StartArray();
    for (const auto& r : partitions) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

struct ProduceResp {
  std::vector<ProduceRespTopic> topics;
  int32_t throttle_time_ms;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("topics");
    writer->StartArray();
    for (const auto& r : topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("throttle_time_ms");
    writer->Int(throttle_time_ms);
    writer->EndObject();
  }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
