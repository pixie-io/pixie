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

struct JoinGroupMember {
  std::string member_id;
  std::string group_instance_id;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("member_id", member_id);
    builder->WriteKV("group_instance_id", group_instance_id);
  }
};

struct JoinGroupProtocol {
  std::string protocol;
  void ToJSON(utils::JSONObjectBuilder* builder) const { builder->WriteKV("protocol", protocol); }
};

struct JoinGroupReq {
  std::string group_id;
  int32_t session_timeout_ms = -1;
  int32_t rebalance_timeout_ms = -1;
  std::string member_id;
  std::string group_instance_id;
  std::string protocol_type;
  std::vector<JoinGroupProtocol> protocols;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("group_id", group_id);
    builder->WriteKV("session_timeout_ms", session_timeout_ms);
    builder->WriteKV("rebalance_timeout_ms", rebalance_timeout_ms);
    builder->WriteKV("member_id", member_id);
    builder->WriteKV("group_instance_id", group_instance_id);
    builder->WriteKV("protocol_type", protocol_type);
    builder->WriteKVArrayRecursive<JoinGroupProtocol>("protocols", protocols);
  }
};

struct JoinGroupResp {
  int32_t throttle_time_ms = 0;
  int16_t error_code = 0;
  int32_t generation_id = -1;
  std::string protocol_type;
  std::string protocol_name;
  std::string leader;
  std::string member_id;
  std::vector<JoinGroupMember> members;

  void ToJSON(utils::JSONObjectBuilder* builder) const {
    builder->WriteKV("throttle_time_ms", throttle_time_ms);
    builder->WriteKV("error_code", error_code);
    builder->WriteKV("generation_id", generation_id);
    builder->WriteKV("protocol_type", protocol_type);
    builder->WriteKV("protocol_name", protocol_name);
    builder->WriteKV("leader", leader);
    builder->WriteKV("member_id", member_id);
    builder->WriteKVArrayRecursive<JoinGroupMember>("members", members);
  }
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
