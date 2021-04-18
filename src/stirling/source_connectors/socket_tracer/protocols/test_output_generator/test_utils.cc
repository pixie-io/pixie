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

#include "src/stirling/source_connectors/socket_tracer/protocols/test_output_generator/test_utils.h"
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <cstring>
#include <fstream>
#include <memory>
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace test_generator_utils {

std::unique_ptr<rapidjson::Value> FlattenWireSharkJSONOutput(const std::string& json_path,
                                                             const std::string& protocol) {
  rapidjson::Document d;
  ReadJSON(json_path, &d);

  auto packets = std::make_unique<rapidjson::Value>(rapidjson::kArrayType);

  for (auto& raw_packet : d.GetArray()) {
    rapidjson::Value& layers = raw_packet["_source"]["layers"];

    for (rapidjson::Value::MemberIterator it = layers.MemberBegin(); it != layers.MemberEnd();
         ++it) {
      std::string_view name(it->name.GetString(), it->name.GetStringLength());
      if (name == protocol) {
        rapidjson::Value& v = it->value;
        packets->PushBack(v.Move(), d.GetAllocator());
      }
    }
  }
  return packets;
}

void ReadJSON(const std::string& json_path, rapidjson::Document* d) {
  std::ifstream ifs(json_path);
  rapidjson::IStreamWrapper isw(ifs);
  d->ParseStream(isw);
}

void WriteJSON(const std::string& output_path, rapidjson::Document* d) {
  std::ofstream ofs(output_path);
  rapidjson::OStreamWrapper osw(ofs);
  rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
  d->Accept(writer);
}

}  // namespace test_generator_utils

namespace mysql {

std::unique_ptr<std::vector<Record>> JSONtoMySQLRecord(const std::string& input_path) {
  rapidjson::Document d;
  test_generator_utils::ReadJSON(input_path, &d);

  auto records = std::make_unique<std::vector<Record>>();

  for (auto& value : d.GetArray()) {
    Record r;
    r.req.cmd = static_cast<Command>(value["req_cmd"].GetString()[0]);
    r.req.msg = value["req_msg"].GetString();
    r.req.timestamp_ns = value["req_timestamp"].GetInt();

    r.resp.status = static_cast<RespStatus>(value["resp_status"].GetString()[0]);
    r.resp.msg = value["resp_msg"].GetString();
    r.resp.timestamp_ns = value["resp_timestamp"].GetInt();

    records->push_back(r);
  }
  return records;
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
