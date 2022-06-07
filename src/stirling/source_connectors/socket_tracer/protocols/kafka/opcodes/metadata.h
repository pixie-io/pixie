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
struct MetadataReqTopic {
  std::string topic_id;
  std::string name;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("topic_id", topic_id);
    builder->WriteKV("name", name);
  }
};

struct MetadataReq {
  std::vector<MetadataReqTopic> topics;
  bool allow_auto_topic_creation = false;
  bool include_cluster_authorized_operations = false;
  bool include_topic_authorized_operations = false;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKVArrayRecursive<MetadataReqTopic>("topics", topics);
    builder->WriteKV("allow_auto_topic_creation", allow_auto_topic_creation);
    builder->WriteKV("include_cluster_authorized_operations",
                     include_cluster_authorized_operations);
    builder->WriteKV("include_topic_authorized_operations", include_topic_authorized_operations);
  }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
