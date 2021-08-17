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

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("index");
    writer->Int(index);
    writer->Key("current_leader_epoch");
    writer->Int(current_leader_epoch);
    writer->Key("fetch_offset");
    writer->Int(fetch_offset);
    writer->Key("last_fetched_epoch");
    writer->Int(last_fetched_epoch);
    writer->Key("log_start_offset");
    writer->Int(log_start_offset);
    writer->Key("partition_max_bytes");
    writer->Int(partition_max_bytes);
    writer->EndObject();
  }
};

struct FetchReqTopic {
  std::string name;
  std::vector<FetchReqPartition> partitions;

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

struct FetchForgottenTopicsData {
  std::string name;
  std::vector<int32_t> partition_indices;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("name");
    writer->String(name.c_str());
    writer->Key("partition_indices");
    writer->StartArray();
    for (const auto& i : partition_indices) {
      writer->Int(i);
    }
    writer->EndArray();
    writer->EndObject();
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

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("replica_id");
    writer->Int(replica_id);
    writer->Key("session_id");
    writer->Int(session_id);
    writer->Key("session_epoch");
    writer->Int(session_epoch);
    writer->Key("topics");
    writer->StartArray();
    for (const auto& r : topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("forgotten_topics");
    writer->StartArray();
    for (const auto& r : forgotten_topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("rack_id");
    writer->String(rack_id.c_str());
    writer->EndObject();
  }
};

struct FetchRespAbortedTransaction {
  int64_t producer_id;
  int64_t first_offset;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("producer_id");
    writer->Int(producer_id);
    writer->Key("first_offset");
    writer->Int(first_offset);
    writer->EndObject();
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

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("index");
    writer->Int(index);
    writer->Key("error_code");
    writer->Int(error_code);
    writer->Key("high_watermark");
    writer->Int(high_watermark);
    writer->Key("last_stable_offset");
    writer->Int(last_stable_offset);
    writer->Key("log_start_offset");
    writer->Int(log_start_offset);
    writer->Key("aborted_transactions");
    writer->StartArray();
    for (const auto& r : aborted_transactions) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("preferred_read_replica");
    writer->Int(preferred_read_replica);
    writer->Key("message_set");
    message_set.ToJSON(writer);
    writer->EndObject();
  }
};

struct FetchRespTopic {
  std::string name;
  std::vector<FetchRespPartition> partitions;

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

struct FetchResp {
  int32_t throttle_time_ms;
  int16_t error_code = 0;
  int32_t session_id = 0;
  std::vector<FetchRespTopic> topics;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("throttle_time_ms");
    writer->Int(throttle_time_ms);
    writer->Key("error_code");
    writer->Int(error_code);
    writer->Key("session_id");
    writer->Int(session_id);
    writer->Key("topics");
    writer->StartArray();
    for (const auto& r : topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
