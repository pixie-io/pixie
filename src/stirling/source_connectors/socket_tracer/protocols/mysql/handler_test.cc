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
#include <utility>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/handler.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

TEST(HandleErrMessage, Basic) {
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  std::deque<Packet> resp_packets = {testutils::GenErr(/* seq_id */ 1, err_resp)};

  Record entry;
  EXPECT_OK_AND_EQ(HandleErrMessage(resp_packets, &entry), ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, RespStatus::kErr);
  EXPECT_EQ(entry.resp.msg, "This is an error.");
}

TEST(HandleOKMessage, Basic) {
  std::deque<Packet> resp_packets = {testutils::GenOK(1)};

  Record entry;
  EXPECT_OK_AND_EQ(HandleOKMessage(resp_packets, &entry), ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, RespStatus::kOK);
}

TEST(HandleResultsetResponse, ValidWithEOF) {
  // Test without CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);

  Record entry;
  EXPECT_OK_AND_EQ(HandleResultsetResponse(resp_packets, &entry, /* binaryresultset */ true,
                                           /* multiresultset */ false),
                   ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, RespStatus::kOK);
  EXPECT_EQ(entry.resp.msg, "Resultset rows = 2");
}

TEST(HandleResultsetResponse, ValidNoEOF) {
  // Test with CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset, true);

  Record entry;
  EXPECT_OK_AND_EQ(HandleResultsetResponse(resp_packets, &entry, /* binaryresultset */ true,
                                           /* multiresultset */ false),
                   ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, RespStatus::kOK);
  EXPECT_EQ(entry.resp.msg, "Resultset rows = 2");
}

TEST(HandleResultsetResponse, NeedsMoreData) {
  // Test for incomplete response.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.pop_back();

  Record entry;
  EXPECT_OK_AND_EQ(HandleResultsetResponse(resp_packets, &entry, /* binaryresultset */ true,
                                           /* multiresultset */ false),
                   ParseState::kNeedsMoreData);
  EXPECT_EQ(entry.resp.status, RespStatus::kUnknown);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleResultsetResponse, InvalidResponse) {
  // Test for invalid response by changing first packet.
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.front() = testutils::GenErr(/* seq_id */ 1, err_resp);

  Record entry;
  State state;
  EXPECT_NOT_OK(HandleResultsetResponse(resp_packets, &entry, /* binaryresultset */ true,
                                        /* multiresultset */ false));
  EXPECT_EQ(entry.resp.status, RespStatus::kUnknown);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtPrepareOKResponse, Valid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);

  Record entry;
  State state;
  EXPECT_OK_AND_EQ(HandleStmtPrepareOKResponse(packets, &state, &entry), ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, RespStatus::kOK);
  EXPECT_EQ(state.prepared_statements.size(), 1);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtPrepareOKResponse, NeedsMoreData) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  // Popping off just one packet would pop off the EOF at the end, but an EOF isn't always required
  // to be present. So the missing of the last EOF shouldn't trigger NeedsMoreData, but rather just
  // a warning if not DeprecateEOF. But popping off two packets should definitely trigger
  // NeedsMoreData.
  packets.pop_back();
  packets.pop_back();

  Record entry;
  State state;
  EXPECT_OK_AND_EQ(HandleStmtPrepareOKResponse(packets, &state, &entry),
                   ParseState::kNeedsMoreData);
  EXPECT_EQ(entry.resp.status, RespStatus::kUnknown);
  EXPECT_EQ(state.prepared_statements.size(), 0);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtPrepareOKResponse, Invalid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  packets.front() = testutils::GenErr(/* seq_id */ 1, err_resp);

  Record entry;
  State state;
  EXPECT_NOT_OK(HandleStmtPrepareOKResponse(packets, &state, &entry));
  EXPECT_EQ(entry.resp.status, RespStatus::kUnknown);
  EXPECT_EQ(state.prepared_statements.size(), 0);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtExecuteRequest, Basic) {
  Packet req_packet = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  PreparedStatement prepared_stmt = testdata::kPreparedStatement;
  int stmt_id = prepared_stmt.response.header.stmt_id;
  std::map<int, PreparedStatement> prepare_map;
  prepare_map.emplace(stmt_id, std::move(prepared_stmt));

  Record entry;
  EXPECT_OK_AND_EQ(HandleStmtExecuteRequest(req_packet, &prepare_map, &entry),
                   ParseState::kSuccess);
  EXPECT_EQ(entry.req.cmd, Command::kStmtExecute);
  EXPECT_EQ(entry.req.msg,
            "query=[SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN "
            "sock_tag "
            "ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
            "tag.name=? GROUP BY id ORDER BY ?] params=[brown, id]");
}

TEST(HandleStringRequest, Basic) {
  Packet req_packet =
      testutils::GenStringRequest(testdata::kStmtPrepareRequest, Command::kStmtPrepare);

  Record entry;
  EXPECT_OK_AND_EQ(HandleStringRequest(req_packet, &entry), ParseState::kSuccess);
  EXPECT_EQ(entry.req.cmd, Command::kStmtPrepare);
  EXPECT_EQ(entry.req.msg,
            "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag "
            "ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
            "tag.name=? GROUP BY id ORDER BY ?");
}

// TODO(chengruizhe): Add failure test cases.

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
