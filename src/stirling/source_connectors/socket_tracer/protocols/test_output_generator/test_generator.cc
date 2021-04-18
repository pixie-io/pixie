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

#include <vector>

#include "rapidjson/document.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/test_output_generator/test_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

namespace {

/**
 * Parse out MySQL request from a JSON mysql packet.
 */
void GenMySQLRequest(const rapidjson::Value& mysql_request, Record* r) {
  const rapidjson::Value& mysql_command = mysql_request["mysql.command"];
  int mysql_command_int;
  CHECK(absl::SimpleAtoi(mysql_command.GetString(), &mysql_command_int));
  Command type = static_cast<Command>(mysql_command_int);
  r->req.cmd = type;
  r->req.timestamp_ns = 0;

  switch (type) {
    case Command::kQuery:
    case Command::kStmtPrepare:
      r->req.msg = mysql_request["mysql.query"].GetString();
      break;
    case Command::kInitDB:
      r->req.msg = mysql_request["mysql.schema"].GetString();
      break;
    case Command::kStmtExecute:
      // TODO(chengruizhe): Pair together StmtExecute with StmtPrepare.
      r->req.msg = "";
      break;
    // TODO(chengruizhe): Add other request commands.
    default:
      r->req.msg = "";
  }
}

/**
 * Generate the corresponding MySQL Records from the WireShark JSON dump.
 */
std::unique_ptr<std::vector<Record>> GenMySQLRecords(const std::string& wireshark_path) {
  auto mysql_packets = test_generator_utils::FlattenWireSharkJSONOutput(wireshark_path, "mysql");
  auto results = std::make_unique<std::vector<Record>>();

  // Iterate through the flattened mysql packets, and form the corresponding records
  rapidjson::SizeType idx = 0;
  while (idx < mysql_packets->Size()) {
    rapidjson::Value& mysql_data = (*mysql_packets)[idx];
    if (mysql_data.HasMember("mysql.request")) {
      Record r;
      GenMySQLRequest(mysql_data["mysql.request"], &r);
      ++idx;

      // If next record is a response, parse it, otherwise there's no response.
      if (idx < mysql_packets->Size()) {
        const rapidjson::Value& next_data = (*mysql_packets)[idx];
        r.resp.timestamp_ns = 0;

        if (!next_data.HasMember("mysql.request")) {
          // Next packet is a response.
          if (next_data.HasMember("mysql.err_code")) {
            r.resp.status = RespStatus::kErr;
            r.resp.msg = next_data["mysql.error"]["message"].GetString();
          } else {
            r.resp.status = RespStatus::kOK;
            // TODO(chengruizhe): Here the resp message can contain resultset or other types of
            // responses. Further parsing is needed. E.g. Need to keep the states of StmtPrepare and
            // fill in with StmtExecute params.
            r.resp.msg = "";
          }
          ++idx;
        } else {
          // Next packet is also a request.
          r.resp.status = RespStatus::kNone;
          r.resp.msg = "";
        }
      } else {
        // This request is the last packet.
        r.resp.status = RespStatus::kNone;
        r.resp.msg = "";
      }

      results->push_back(r);
    } else {
      ++idx;
    }
  }

  return results;
}

/**
 * Takes in a vector of mysql::Records, and generates a trimmed JSON file used as expected test
 * output.
 */
void MySQLRecordtoJSON(const std::string& output_path,
                       std::unique_ptr<std::vector<Record>> records) {
  rapidjson::Document d;
  d.SetArray();
  rapidjson::Document::AllocatorType& a = d.GetAllocator();

  for (auto const& r : *records) {
    rapidjson::Value mysql_record(rapidjson::kObjectType);

    rapidjson::Value request_cmd;
    char req_cmd_char = static_cast<char>(r.req.cmd);
    request_cmd.SetString(&req_cmd_char, a);
    mysql_record.AddMember("req_cmd", request_cmd, a);

    rapidjson::Value request_msg;
    request_msg.SetString(r.req.msg.c_str(), a);
    mysql_record.AddMember("req_msg", request_msg, a);

    mysql_record.AddMember("req_timestamp", rapidjson::Value(r.req.timestamp_ns), a);

    rapidjson::Value response_status;
    char resp_status_char = static_cast<char>(r.resp.status);
    response_status.SetString(&resp_status_char, a);
    mysql_record.AddMember("resp_status", response_status, a);

    rapidjson::Value response_msg;
    response_msg.SetString(r.resp.msg.c_str(), a);
    mysql_record.AddMember("resp_msg", response_msg, a);

    mysql_record.AddMember("resp_timestamp", rapidjson::Value(r.resp.timestamp_ns), a);

    d.PushBack(mysql_record.Move(), a);
  }

  test_generator_utils::WriteJSON(output_path, &d);
}

}  // namespace

/**
 * GenMySQLTestOutput converts raw traffic captured by Tshark into trimmed JSON format that matches
 * MySQL::Record.
 * @param wireshark_path: Path to output of Tshark script.
 * @param output_path: Path to write the trimmed JSON file to.
 */
void GenMySQLTestOutput(const std::string& wireshark_path, const std::string& output_path) {
  auto records = GenMySQLRecords(wireshark_path);
  MySQLRecordtoJSON(output_path, std::move(records));
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);
  px::stirling::protocols::mysql::GenMySQLTestOutput(argv[1], argv[2]);
  return 0;
}
