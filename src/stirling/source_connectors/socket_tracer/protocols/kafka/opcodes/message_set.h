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

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

struct RecordMessage {
  std::string key;
  std::string value;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("key");
    writer->String(key.c_str());
    writer->Key("value");
    writer->String(value.c_str());
    writer->EndObject();
  }
};

struct RecordBatch {
  std::vector<RecordMessage> records;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("records");
    writer->StartArray();
    for (const auto& r : records) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

struct MessageSet {
  int64_t size = 0;
  std::vector<RecordBatch> record_batches;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer,
              bool omit_record_batches = false) const {
    writer->StartObject();
    writer->Key("size");
    writer->Int(size);
    if (!omit_record_batches) {
      writer->Key("record_batches");
      writer->StartArray();
      for (const auto& r : record_batches) {
        r.ToJSON(writer);
      }
      writer->EndArray();
    }
    writer->EndObject();
  }
};

inline bool operator==(const RecordMessage& lhs, const RecordMessage& rhs) {
  return lhs.key == rhs.key && lhs.value == rhs.value;
}

inline bool operator!=(const RecordMessage& lhs, const RecordMessage& rhs) { return !(lhs == rhs); }

inline bool operator==(const RecordBatch& lhs, const RecordBatch& rhs) {
  if (lhs.records.size() != rhs.records.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.records.size(); ++i) {
    if (lhs.records[i] != rhs.records[i]) {
      return false;
    }
  }
  return true;
}

inline bool operator!=(const RecordBatch& lhs, const RecordBatch& rhs) { return !(lhs == rhs); }

inline bool operator==(const MessageSet& lhs, const MessageSet& rhs) {
  if (lhs.record_batches.size() != rhs.record_batches.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.record_batches.size(); ++i) {
    if (lhs.record_batches[i] != rhs.record_batches[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
