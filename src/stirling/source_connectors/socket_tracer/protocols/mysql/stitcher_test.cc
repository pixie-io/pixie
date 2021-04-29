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

#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

bool operator==(const Record& lhs, const Record& rhs) {
  return lhs.req.cmd == rhs.req.cmd && lhs.req.msg == rhs.req.msg &&
         lhs.req.timestamp_ns == rhs.req.timestamp_ns && lhs.resp.status == rhs.resp.status &&
         lhs.resp.msg == rhs.resp.msg && lhs.resp.timestamp_ns == rhs.resp.timestamp_ns;
}

TEST(StitcherTest, TestProcessStmtPrepareOK) {
  // Test setup.
  Packet req = testutils::GenStringRequest(testdata::kStmtPrepareRequest, Command::kStmtPrepare);
  std::deque<Packet> ok_resp_packets =
      testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  State state{std::map<int, PreparedStatement>()};

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(ProcessStmtPrepare(req, ok_resp_packets, &state, &entry), ParseState::kSuccess);

  // Check resulting state and entries.
  auto iter = state.prepared_statements.find(testdata::kStmtID);
  EXPECT_TRUE(iter != state.prepared_statements.end());
  Record expected_entry{.req = {Command::kStmtPrepare, testdata::kStmtPrepareRequest.msg, 0},
                        .resp = {RespStatus::kOK, "", 0}};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessStmtPrepareErr) {
  // Test setup.
  Packet req = testutils::GenStringRequest(testdata::kStmtPrepareRequest, Command::kStmtPrepare);
  std::deque<Packet> err_resp_packets;
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  err_resp_packets.emplace_back(testutils::GenErr(/* seq_id */ 1, err_resp));
  State state{std::map<int, PreparedStatement>()};

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(ProcessStmtPrepare(req, err_resp_packets, &state, &entry), ParseState::kSuccess);

  // Check resulting state and entries.
  auto iter = state.prepared_statements.find(testdata::kStmtID);
  EXPECT_EQ(iter, state.prepared_statements.end());
  Record expected_err_entry{.req = {Command::kStmtPrepare, testdata::kStmtPrepareRequest.msg, 0},
                            .resp = {RespStatus::kErr, "This is an error.", 0}};
  EXPECT_EQ(expected_err_entry, entry);
}

TEST(StitcherTest, TestProcessStmtExecute) {
  // Test setup.
  Packet req = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  std::deque<Packet> resultset = testutils::GenResultset(testdata::kStmtExecuteResultset);
  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(ProcessStmtExecute(req, resultset, &state, &entry), ParseState::kSuccess);

  // Check resulting state and entries.
  Record expected_resultset_entry{
      .req = {Command::kStmtExecute,
              "query=[SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
              "JOIN sock_tag ON "
              "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
              "tag.name=? "
              "GROUP "
              "BY id ORDER BY ?] "
              "params=[brown, id]",
              0},
      .resp = {RespStatus::kOK, "Resultset rows = 2", 0}};
  EXPECT_EQ(expected_resultset_entry, entry);
}

TEST(StitcherTest, TestProcessStmtSendLongData) {
  // Test setup.
  // TODO(oazizi): Not a real COM_STMT_SEND_LONG_DATA. Need to replace with a real capture.
  Packet req = testutils::GenStringRequest(StringRequest{""}, Command::kStmtSendLongData);
  std::deque<Packet> resp_packets = {};
  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(ProcessStmtSendLongData(req, resp_packets, &state, &entry),
                   ParseState::kSuccess);

  // Check the resulting entries and state.
  Record expected_entry{.req = {Command::kStmtSendLongData, "", 0},
                        .resp = {RespStatus::kNone, "", 0}};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessStmtClose) {
  // Test setup.
  Packet req = testutils::GenStmtCloseRequest(testdata::kStmtCloseRequest);
  std::deque<Packet> resp_packets = {};
  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(ProcessStmtClose(req, resp_packets, &state, &entry), ParseState::kSuccess);

  // Check the resulting entries and state.
  Record expected_entry{.req = {Command::kStmtClose, "", 0}, .resp = {RespStatus::kNone, "", 0}};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessQuery) {
  // Test setup.
  Packet req = testutils::GenStringRequest(testdata::kQueryRequest, Command::kQuery);
  std::deque<Packet> resultset = testutils::GenResultset(testdata::kQueryResultset);

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(ProcessQuery(req, resultset, &entry), ParseState::kSuccess);

  // Check resulting state and entries.
  Record expected_resultset_entry{.req = {Command::kQuery, "SELECT name FROM tag;", 0},
                                  .resp = {RespStatus::kOK, "Resultset rows = 3", 0}};
  EXPECT_EQ(expected_resultset_entry, entry);
}

TEST(StitcherTest, ProcessRequestWithBasicResponse) {
  // Test setup.
  // Ping is a request that always has a response OK.
  Packet req = testutils::GenStringRequest(StringRequest(), Command::kPing);
  std::deque<Packet> resp_packets = {testutils::GenOK(/*seq_id*/ 1)};

  // Run function-under-test.
  Record entry;
  EXPECT_OK_AND_EQ(
      ProcessRequestWithBasicResponse(req, /* string_req */ false, resp_packets, &entry),
      ParseState::kSuccess);

  // Check resulting state and entries.
  Record expected_entry{.req = {Command::kPing, "", 0}, .resp = {RespStatus::kOK, "", 0}};
  EXPECT_EQ(expected_entry, entry);
}

TEST(ProcessMySQLPacketsTest, OldResponses) {
  uint64_t t = 0;

  // First two packets are dangling, and pre-date the request below.
  Packet resp0 = testutils::GenOK(/*seq_id*/ 1);
  resp0.timestamp_ns = t++;

  Packet resp1 = testutils::GenOK(/*seq_id*/ 1);
  resp1.timestamp_ns = t++;

  // This is the main request, with a well-formed response below.
  Packet req = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  req.timestamp_ns = t++;

  std::deque<Packet> responses = testutils::GenResultset(testdata::kStmtExecuteResultset);
  for (auto& p : responses) {
    p.timestamp_ns = t++;
  }

  // Now prepend the two dangling packets to the responses.
  responses.push_front(resp1);
  responses.push_front(resp0);

  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

  std::deque<Packet> requests = {req};
  RecordsWithErrorCount<Record> result = ProcessMySQLPackets(&requests, &responses, &state);
  EXPECT_EQ(result.records.size(), 1);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(requests.size(), 0);
  EXPECT_EQ(responses.size(), 0);
}

TEST(ProcessMySQLPacketsTest, NonMySQLTraffic1) {
  Packet p0;
  p0.sequence_id = 0;
  p0.msg = "\x03this is wrongly classified as a mysql COM_QUERY because of the first byte";
  p0.timestamp_ns = 0;

  Packet p1;
  p1.sequence_id = 0;
  p1.msg = "Not a valid mysql response. Has the wrong sequence ID, even.";
  p1.timestamp_ns = 1;

  std::deque<Packet> requests = {p0};
  std::deque<Packet> responses = {p1};
  State state{.prepared_statements = std::map<int, PreparedStatement>(), .active = false};

  RecordsWithErrorCount<Record> result = ProcessMySQLPackets(&requests, &responses, &state);
  EXPECT_EQ(result.records.size(), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(requests.size(), 0);
  EXPECT_EQ(responses.size(), 1);
  // Note that the response is not consumed because of its unexpected sequence ID.
}

TEST(ProcessMySQLPacketsTest, NonMySQLTraffic2) {
  Packet p0;
  p0.sequence_id = 0;
  p0.msg = "\x03this is wrongly classified as a mysql COM_QUERY because of the first byte";
  p0.timestamp_ns = 0;

  Packet p1;
  p1.sequence_id = 1;
  p1.msg = "Not a valid mysql response. But has the right sequence ID.";
  p1.timestamp_ns = 1;

  std::deque<Packet> requests = {p0};
  std::deque<Packet> responses = {p1};
  State state{.prepared_statements = std::map<int, PreparedStatement>(), .active = false};

  RecordsWithErrorCount<Record> result = ProcessMySQLPackets(&requests, &responses, &state);
  EXPECT_EQ(result.records.size(), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(requests.size(), 0);
  EXPECT_EQ(responses.size(), 0);
}

TEST(ProcessMySQLPacketsTest, NonMySQLTraffic3) {
  Packet p;
  p.sequence_id = 0;
  p.timestamp_ns = 0;
  // This packet is particularly tricky to filter out, because it is a StmtSendLongData (0x18),
  // Random bytes easily match to StmtSendLongData because it has no response,
  // and the command can carry an arbitrary payload.
  p.msg = ConstStringView("\x18\x16\x04\x01\x96\x00\x00\x00\x01\x01\x6F\x00\x33");

  std::deque<Packet> requests = {p};
  std::deque<Packet> responses = {};
  State state{.prepared_statements = std::map<int, PreparedStatement>(), .active = false};

  RecordsWithErrorCount<Record> result = ProcessMySQLPackets(&requests, &responses, &state);
  EXPECT_EQ(result.records.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(requests.size(), 0);
  EXPECT_EQ(responses.size(), 0);
}

// TODO(chengruizhe): Add test cases for inputs that would return Status error.

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
