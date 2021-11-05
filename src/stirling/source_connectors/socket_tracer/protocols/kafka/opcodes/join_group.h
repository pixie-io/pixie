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

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("member_id");
    writer->String(member_id.c_str());
    writer->Key("group_instance_id");
    writer->String(group_instance_id.c_str());
    writer->EndObject();
  }
};

struct JoinGroupProtocol {
  std::string protocol;
  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("protocol");
    writer->String(protocol.c_str());
    writer->EndObject();
  }
};

struct JoinGroupReq {
  std::string group_id;
  int32_t session_timeout_ms = -1;
  int32_t rebalance_timeout_ms = -1;
  std::string member_id;
  std::string group_instance_id;
  std::string protocol_type;
  std::vector<JoinGroupProtocol> protocols;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("group_id");
    writer->String(group_id.c_str());
    writer->Key("session_timeout_ms");
    writer->Int(session_timeout_ms);
    writer->Key("rebalance_timeout_ms");
    writer->Int(rebalance_timeout_ms);
    writer->Key("member_id");
    writer->String(member_id.c_str());
    writer->Key("group_instance_id");
    writer->String(group_instance_id.c_str());
    writer->Key("protocol_type");
    writer->String(protocol_type.c_str());
    writer->Key("protocols");
    writer->StartArray();
    for (const auto& r : protocols) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
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

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("throttle_time_ms");
    writer->Int(throttle_time_ms);
    writer->Key("error_code");
    writer->Int(error_code);
    writer->Key("generation_id");
    writer->Int(generation_id);
    writer->Key("protocol_type");
    writer->String(protocol_type.c_str());
    writer->Key("protocol_name");
    writer->String(protocol_name.c_str());
    writer->Key("leader");
    writer->String(leader.c_str());
    writer->Key("member_id");
    writer->String(member_id.c_str());
    writer->Key("members");
    writer->StartArray();
    for (const auto& r : members) {
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
