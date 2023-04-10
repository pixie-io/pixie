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

#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "src/shared/metadata/metadata.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

#include "src/common/testing/testing.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"
#include "src/stirling/source_connectors/socket_tracer/testing/http2_stream_generator.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_connector_friend.h"
#include "src/stirling/testing/common.h"

// ********* DEPRECATED: DO NOT ADD NEW TESTS TO THIS FILE.
// Protocol specific test coverage should be added to this relevant protocol parser.

namespace px {
namespace stirling {

namespace mysql = protocols::mysql;
namespace cass = protocols::cass;

using ::testing::ElementsAre;

using ::px::stirling::testing::RecordBatchSizeIs;

using RecordBatch = types::ColumnWrapperRecordBatch;

//-----------------------------------------------------------------------------
// Test data
//-----------------------------------------------------------------------------

std::vector<std::string> PacketsToRaw(const std::deque<mysql::Packet>& packets) {
  std::vector<std::string> res;
  for (const auto& p : packets) {
    res.push_back(mysql::testutils::GenRawPacket(p));
  }
  return res;
}

// NOLINTNEXTLINE : runtime/string.
const std::string kMySQLStmtPrepareReq =
    mysql::testutils::GenRawPacket(mysql::testutils::GenStringRequest(
        mysql::testdata::kStmtPrepareRequest, mysql::Command::kStmtPrepare));

const std::vector<std::string> kMySQLStmtPrepareResp =
    PacketsToRaw(mysql::testutils::GenStmtPrepareOKResponse(mysql::testdata::kStmtPrepareResponse));

// NOLINTNEXTLINE : runtime/string.
const std::string kMySQLStmtExecuteReq = mysql::testutils::GenRawPacket(
    mysql::testutils::GenStmtExecuteRequest(mysql::testdata::kStmtExecuteRequest));

const std::vector<std::string> kMySQLStmtExecuteResp =
    PacketsToRaw(mysql::testutils::GenResultset(mysql::testdata::kStmtExecuteResultset));

// NOLINTNEXTLINE : runtime/string.
const std::string kMySQLStmtCloseReq = mysql::testutils::GenRawPacket(
    mysql::testutils::GenStmtCloseRequest(mysql::testdata::kStmtCloseRequest));

// NOLINTNEXTLINE : runtime/string.
const std::string kMySQLErrResp = mysql::testutils::GenRawPacket(mysql::testutils::GenErr(
    1, mysql::ErrResponse{.error_code = 1096, .error_message = "This is an error."}));

// NOLINTNEXTLINE : runtime/string.
const std::string kMySQLQueryReq = mysql::testutils::GenRawPacket(
    mysql::testutils::GenStringRequest(mysql::testdata::kQueryRequest, mysql::Command::kQuery));

const std::vector<std::string> kMySQLQueryResp =
    PacketsToRaw(mysql::testutils::GenResultset(mysql::testdata::kQueryResultset));

//-----------------------------------------------------------------------------
// Test data
//-----------------------------------------------------------------------------

class SocketTraceConnectorTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kASID = 0;

  void SetUp() override {
    // Create and configure the connector.
    connector_ = SocketTraceConnectorFriend::Create("socket_trace_connector");
    source_ = dynamic_cast<SocketTraceConnectorFriend*>(connector_.get());
    ASSERT_NE(nullptr, source_);

    ctx_ = std::make_unique<SystemWideStandaloneContext>();

    // Tell the source to use our injected clock for getting the current time.
    source_->test_only_set_now_fn(
        [this]() { return testing::NanosToTimePoint(mock_clock_.now()); });

    // Set the CIDR for HTTP2ServerTest, which would otherwise not output any data,
    // because it would think the server is in the cluster.
    PX_CHECK_OK(ctx_->SetClusterCIDR("1.2.3.4/32"));

    // Because some tests change the inactivity duration, make sure to reset it here for each test.
    ConnTracker::set_inactivity_duration(ConnTracker::kDefaultInactivityDuration);

    FLAGS_stirling_check_proc_for_conn_close = false;
    FLAGS_stirling_conn_stats_sampling_ratio = 1;

    data_tables_ = std::make_unique<DataTables>(SocketTraceConnector::kTables);
    connector_->set_data_tables(data_tables_->tables());

    // For convenience.
    http_table_ = (*data_tables_)[SocketTraceConnector::kHTTPTableNum];
    mysql_table_ = (*data_tables_)[SocketTraceConnector::kMySQLTableNum];
    cql_table_ = (*data_tables_)[SocketTraceConnector::kCQLTableNum];
  }

  std::unique_ptr<DataTables> data_tables_;
  DataTable* http_table_;
  DataTable* mysql_table_;
  DataTable* cql_table_;

  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnectorFriend* source_ = nullptr;
  std::unique_ptr<SystemWideStandaloneContext> ctx_;
  testing::MockClock mock_clock_;

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;
  static constexpr int kCQLTableNum = SocketTraceConnector::kCQLTableNum;
};

auto ToStringVector(const types::SharedColumnWrapper& col) {
  std::vector<std::string> result;
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<types::StringValue>(i));
  }
  return result;
}

template <typename TValueType>
auto ToIntVector(const types::SharedColumnWrapper& col) {
  std::vector<int64_t> result;
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<TValueType>(i).val);
  }
  return result;
}

//-----------------------------------------------------------------------------
// MySQL specific tests
//-----------------------------------------------------------------------------

TEST_F(SocketTraceConnectorTest, MySQLPrepareExecuteClose) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> prepare_req_event =
      event_gen.InitSendEvent<kProtocolMySQL>(kMySQLStmtPrepareReq);
  std::vector<std::unique_ptr<SocketDataEvent>> prepare_resp_events;
  for (std::string resp_packet : kMySQLStmtPrepareResp) {
    prepare_resp_events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(resp_packet));
  }

  std::unique_ptr<SocketDataEvent> execute_req_event =
      event_gen.InitSendEvent<kProtocolMySQL>(kMySQLStmtExecuteReq);
  std::vector<std::unique_ptr<SocketDataEvent>> execute_resp_events;
  for (std::string resp_packet : kMySQLStmtExecuteResp) {
    execute_resp_events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(resp_packet));
  }

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(prepare_req_event));
  for (auto& prepare_resp_event : prepare_resp_events) {
    source_->AcceptDataEvent(std::move(prepare_resp_event));
  }

  source_->AcceptDataEvent(std::move(execute_req_event));
  for (auto& execute_resp_event : execute_resp_events) {
    source_->AcceptDataEvent(std::move(execute_resp_event));
  }

  std::vector<TaggedRecordBatch> tablets;

  connector_->TransferData(ctx_.get());
  tablets = mysql_table_->ConsumeRecords();

  {
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
    EXPECT_THAT(record_batch, RecordBatchSizeIs(2));

    std::string expected_entry0 =
        "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
        "JOIN sock_tag ON "
        "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=? "
        "GROUP "
        "BY id ORDER BY ?";

    std::string expected_entry1 =
        "query=[SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
        "JOIN sock_tag ON "
        "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=? "
        "GROUP "
        "BY id ORDER BY ?] params=[brown, id]";

    EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]),
                ElementsAre(expected_entry0, expected_entry1));
    EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]),
                ElementsAre("", "Resultset rows = 2"));
    // In test environment, latencies are simply the number of packets in the response.
    // StmtPrepare resp has 7 response packets: 1 header + 2 col defs + 1 EOF + 2 param defs + 1
    // EOF. StmtExecute resp has 7 response packets: 1 header + 2 col defs + 1 EOF + 2 rows + 1 EOF.
    EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(7, 7));
  }

  // Test execute fail after close. It should create an entry with the Error.
  std::unique_ptr<SocketDataEvent> close_req_event =
      event_gen.InitSendEvent<kProtocolMySQL>(kMySQLStmtCloseReq);
  std::unique_ptr<SocketDataEvent> execute_req_event2 =
      event_gen.InitSendEvent<kProtocolMySQL>(kMySQLStmtExecuteReq);
  std::unique_ptr<SocketDataEvent> execute_resp_event2 =
      event_gen.InitRecvEvent<kProtocolMySQL>(kMySQLErrResp);

  source_->AcceptDataEvent(std::move(close_req_event));
  source_->AcceptDataEvent(std::move(execute_req_event2));
  source_->AcceptDataEvent(std::move(execute_resp_event2));

  connector_->TransferData(ctx_.get());
  tablets = mysql_table_->ConsumeRecords();

  {
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
    EXPECT_THAT(record_batch, RecordBatchSizeIs(2));
    EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]),
                ElementsAre("", "Execute stmt_id=2."));
    EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]),
                ElementsAre("", "This is an error."));
    // In test environment, latencies are simply the number of packets in the response.
    // StmtClose resp has 0 response packets.
    // StmtExecute resp has 1 response packet: 1 error.
    EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(0, 1));
  }
}

TEST_F(SocketTraceConnectorTest, MySQLQuery) {
  testing::EventGenerator event_gen(&mock_clock_);

  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> query_req_event =
      event_gen.InitSendEvent<kProtocolMySQL>(kMySQLQueryReq);
  std::vector<std::unique_ptr<SocketDataEvent>> query_resp_events;
  for (std::string resp_packet : kMySQLQueryResp) {
    query_resp_events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(resp_packet));
  }

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(query_req_event));
  for (auto& query_resp_event : query_resp_events) {
    source_->AcceptDataEvent(std::move(query_resp_event));
  }

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  EXPECT_THAT(record_batch, RecordBatchSizeIs(1));

  EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]), ElementsAre("SELECT name FROM tag;"));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]), ElementsAre("Resultset rows = 3"));
  // In test environment, latencies are simply the number of packets in the response.
  // In this case 7 response packets: 1 header + 1 col defs + 1 EOF + 3 rows + 1 EOF.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(7));
}

TEST_F(SocketTraceConnectorTest, MySQLMultipleCommands) {
  testing::EventGenerator event_gen(&mock_clock_);

  struct socket_control_event_t conn = event_gen.InitConn();

  // The following is a captured trace while running a script on a real instance of MySQL.
  std::vector<std::unique_ptr<SocketDataEvent>> events;
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(
      {ConstStringView("\x21\x00\x00\x00"
                       "\x03"
                       "select @@version_comment limit 1")}));
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>({ConstStringView(
      "\x01\x00\x00\x01"
      "\x01\x27\x00\x00\x02\x03"
      "def"
      "\x00\x00\x00\x11"
      "@@version_comment"
      "\x00\x0C\x21\x00\x18\x00\x00\x00\xFD\x00\x00\x1F\x00\x00\x09\x00\x00\x03\x08"
      "(Ubuntu)"
      "\x07\x00\x00\x04\xFE\x00\x00\x02\x00\x00\x00")}));
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(
      {ConstStringView("\x22\x00\x00\x00"
                       "\x03"
                       "DROP DATABASE IF EXISTS employees")}));
  events.push_back(
      event_gen.InitRecvEvent<kProtocolMySQL>({ConstStringView("\x07\x00\x00\x01"
                                                               "\x00\x00\x00\x02\x01\x00\x00")}));
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(
      {ConstStringView("\x28\x00\x00\x00"
                       "\x03"
                       "CREATE DATABASE IF NOT EXISTS employees")}));
  events.push_back(
      event_gen.InitRecvEvent<kProtocolMySQL>({ConstStringView("\x07\x00\x00\x01"
                                                               "\x00\x01\x00\x02\x00\x00\x00")}));
  events.push_back(
      event_gen.InitSendEvent<kProtocolMySQL>({ConstStringView("\x12\x00\x00\x00"
                                                               "\x03"
                                                               "SELECT DATABASE()")}));
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
      {ConstStringView("\x01\x00\x00\x01"
                       "\x01\x20\x00\x00\x02\x03"
                       "def"
                       "\x00\x00\x00\x0A"
                       "DATABASE()"
                       "\x00\x0C\x21\x00\x66\x00\x00\x00\xFD\x00\x00\x1F\x00\x00\x01\x00\x00\x03"
                       "\xFB\x07\x00\x00\x04\xFE\x00\x00\x02\x00\x00\x00")}));
  events.push_back(
      event_gen.InitSendEvent<kProtocolMySQL>({ConstStringView("\x0A\x00\x00\x00"
                                                               "\x02"
                                                               "employees")}));
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
      {ConstStringView("\x15\x00\x00\x01"
                       "\x00\x00\x00\x02\x40\x00\x00\x00\x0C\x01\x0A\x09"
                       "employees")}));
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(
      {ConstStringView("\x2f\x00\x00\x00"
                       "\x03"
                       "SELECT 'CREATING DATABASE STRUCTURE' as 'INFO'")}));
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>({ConstStringView(
      "\x01\x00\x00\x01"
      "\x01\x1A\x00\x00\x02\x03"
      "def"
      "\x00\x00\x00\x04"
      "INFO"
      "\x00\x0C\x21\x00\x51\x00\x00\x00\xFD\x01\x00\x1F\x00\x00\x1C\x00\x00\x03\x1B"
      "CREATING DATABASE STRUCTURE"
      "\x07\x00\x00\x04\xFE\x00\x00\x02\x00\x00\x00")}));
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(
      {ConstStringView("\xC1\x00\x00\x00"
                       "\x03"
                       "DROP TABLE IF EXISTS dept_emp,\n"
                       "                     dept_manager,\n"
                       "                     titles,\n"
                       "                     salaries, \n"
                       "                     employees, \n"
                       "                     departments")}));
  events.push_back(
      event_gen.InitRecvEvent<kProtocolMySQL>({ConstStringView("\x07\x00\x00\x01"
                                                               "\x00\x00\x00\x02\x00\x06\x00")}));
  events.push_back(
      event_gen.InitSendEvent<kProtocolMySQL>({ConstStringView("\x1C\x00\x00\x00"
                                                               "\x03"
                                                               "set storage_engine = InnoDB")}));
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
      {ConstStringView("\x31\x00\x00\x01"
                       "\xFF\xA9\x04\x23"
                       "HY000"
                       "Unknown system variable 'storage_engine'")}));
  events.push_back(
      event_gen.InitSendEvent<kProtocolMySQL>({ConstStringView("\x01\x00\x00\x00"
                                                               "\x01")}));

  source_->AcceptControlEvent(conn);
  for (auto& event : events) {
    source_->AcceptDataEvent(std::move(event));
  }

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  EXPECT_THAT(record_batch, RecordBatchSizeIs(9));

  // In this test environment, latencies are the number of events.

  int idx = 0;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "select @@version_comment limit 1");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "DROP DATABASE IF EXISTS employees");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "CREATE DATABASE IF NOT EXISTS employees");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "SELECT DATABASE()");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "employees");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kInitDB));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "SELECT 'CREATING DATABASE STRUCTURE' as 'INFO'");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "DROP TABLE IF EXISTS dept_emp,\n                     dept_manager,\n                  "
            "   titles,\n                     salaries, \n                     employees, \n       "
            "              departments");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "set storage_engine = InnoDB");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx),
            "Unknown system variable 'storage_engine'");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx).val, 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuit));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  // Not checking latency since connection ended.
}

// Inspired from real traced query.
// Number of resultset rows is large enough to cause a sequence ID rollover.
TEST_F(SocketTraceConnectorTest, MySQLQueryWithLargeResultset) {
  testing::EventGenerator event_gen(&mock_clock_);

  struct socket_control_event_t conn = event_gen.InitConn();

  // The following is a captured trace while running a script on a real instance of MySQL.
  std::vector<std::unique_ptr<SocketDataEvent>> events;
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(mysql::testutils::GenRequestPacket(
      mysql::Command::kQuery, "SELECT emp_no FROM employees WHERE emp_no < 15000;")));

  // Sequence ID of zero is the request.
  int seq_id = 1;

  // First packet: number of columns in the query.
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
      mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(1))));
  // The column def packet (a bunch of length-encoded strings).
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
      seq_id, mysql::testutils::GenColDefinition(
                  seq_id, mysql::ColDefinition{.catalog = "def",
                                               .schema = "employees",
                                               .table = "employees",
                                               .org_table = "employees",
                                               .name = "emp_no",
                                               .org_name = "emp_no",
                                               .next_length = 12,
                                               .character_set = 0x3f,
                                               .column_length = 11,
                                               .column_type = mysql::ColType::kLong,
                                               .flags = 0x5003,
                                               .decimals = 0})
                  .msg)));
  ++seq_id;
  // A bunch of resultset rows.
  for (int id = 10001; id < 19999; ++id) {
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
        seq_id++, mysql::testutils::LengthEncodedString(std::to_string(id)))));
  }
  // Final OK/EOF packet.
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
      mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\xFE\x00\x00\x02\x00\x00\x00"))));

  source_->AcceptControlEvent(conn);
  for (auto& event : events) {
    source_->AcceptDataEvent(std::move(event));
  }

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));
  int idx = 0;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "SELECT emp_no FROM employees WHERE emp_no < 15000;");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 9998");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx).val, 10001);
}

// Inspired from real traced query that produces a multi-resultset:
//    CREATE TEMPORARY TABLE ins ( id INT );
//    DROP PROCEDURE IF EXISTS multi;
//    DELIMITER $$
//    CREATE PROCEDURE multi() BEGIN
//      SELECT 1;
//      SELECT 1;
//      INSERT INTO ins VALUES (1);
//      INSERT INTO ins VALUES (2);
//    END$$
//    DELIMITER ;
//
//    CALL multi();
//    DROP TABLE ins;
TEST_F(SocketTraceConnectorTest, MySQLMultiResultset) {
  testing::EventGenerator event_gen(&mock_clock_);

  struct socket_control_event_t conn = event_gen.InitConn();

  // The following is a captured trace while running a script on a real instance of MySQL.
  std::vector<std::unique_ptr<SocketDataEvent>> events;
  events.push_back(event_gen.InitSendEvent<kProtocolMySQL>(
      mysql::testutils::GenRequestPacket(mysql::Command::kQuery, "CALL multi()")));

  // Sequence ID of zero is the request.
  int seq_id = 1;

  // First resultset.
  {
    // First packet: number of columns in the query.
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(1))));
    // The column def packet (a bunch of length-encoded strings).
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
        seq_id++,
        mysql::testutils::LengthEncodedString("def") +
            ConstString(
                "\x00\x00\x00\x01\x31\x00\x0C\x3F\x00\x01\x00\x00\x00\x08\x81\x00\x00\x00\x00"))));
    // A resultset row.
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedString("1"))));
    // OK/EOF packet with SERVER_MORE_RESULTS_EXISTS flag set.
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\xFE\x00\x00\x0A\x00\x00\x00"))));
  }

  // Second resultset.
  {
    // First packet: number of columns in the query.
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(1))));
    // The column def packet (a bunch of length-encoded strings).
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
        seq_id++,
        mysql::testutils::LengthEncodedString("def") +
            ConstString(
                "\x00\x00\x00\x01\x31\x00\x0C\x3F\x00\x01\x00\x00\x00\x08\x81\x00\x00\x00\x00"))));
    // A resultset row.
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedString("1"))));
    // OK/EOF packet with SERVER_MORE_RESULTS_EXISTS flag set.
    events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\xFE\x00\x00\x0A\x00\x00\x00"))));
  }

  // Final OK packet, signaling end of multi-resultset.
  events.push_back(event_gen.InitRecvEvent<kProtocolMySQL>(
      mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\x00\x01\x00\x02\x00\x00\x00"))));

  source_->AcceptControlEvent(conn);
  for (auto& event : events) {
    source_->AcceptDataEvent(std::move(event));
  }

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));
  int idx = 0;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "CALL multi()");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx),
            "Resultset rows = 1, Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::Command::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx).val, 9);
}

//-----------------------------------------------------------------------------
// Cassandra/CQL specific tests
//-----------------------------------------------------------------------------

TEST_F(SocketTraceConnectorTest, CQLQuery) {
  using cass::testutils::CreateCQLEvent;

  // QUERY request from client.
  // Contains: SELECT * FROM system.peers
  constexpr uint8_t kQueryReq[] = {0x00, 0x00, 0x00, 0x1a, 0x53, 0x45, 0x4c, 0x45, 0x43,
                                   0x54, 0x20, 0x2a, 0x20, 0x46, 0x52, 0x4f, 0x4d, 0x20,
                                   0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e, 0x70, 0x65,
                                   0x65, 0x72, 0x73, 0x00, 0x01, 0x00};

  // RESULT response to query kQueryReq above.
  // Result contains 9 columns, and 0 rows. Columns are:
  // peer,data_center,host_id,preferred_ip,rack,release_version,rpc_address,schema_version,tokens
  constexpr uint8_t kResultResp[] = {
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00, 0x06,
      0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x00, 0x05, 0x70, 0x65, 0x65, 0x72, 0x73, 0x00,
      0x04, 0x70, 0x65, 0x65, 0x72, 0x00, 0x10, 0x00, 0x0b, 0x64, 0x61, 0x74, 0x61, 0x5f,
      0x63, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x00, 0x0d, 0x00, 0x07, 0x68, 0x6f, 0x73, 0x74,
      0x5f, 0x69, 0x64, 0x00, 0x0c, 0x00, 0x0c, 0x70, 0x72, 0x65, 0x66, 0x65, 0x72, 0x72,
      0x65, 0x64, 0x5f, 0x69, 0x70, 0x00, 0x10, 0x00, 0x04, 0x72, 0x61, 0x63, 0x6b, 0x00,
      0x0d, 0x00, 0x0f, 0x72, 0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x5f, 0x76, 0x65, 0x72,
      0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0d, 0x00, 0x0b, 0x72, 0x70, 0x63, 0x5f, 0x61, 0x64,
      0x64, 0x72, 0x65, 0x73, 0x73, 0x00, 0x10, 0x00, 0x0e, 0x73, 0x63, 0x68, 0x65, 0x6d,
      0x61, 0x5f, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0c, 0x00, 0x06, 0x74,
      0x6f, 0x6b, 0x65, 0x6e, 0x73, 0x00, 0x22, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00};

  testing::EventGenerator event_gen(&mock_clock_);

  struct socket_control_event_t conn = event_gen.InitConn();

  // Any unique number will do.
  uint16_t stream = 2;
  std::unique_ptr<SocketDataEvent> query_req_event =
      event_gen.InitSendEvent<kProtocolCQL>(CreateCQLEvent(cass::ReqOp::kQuery, kQueryReq, stream));
  std::unique_ptr<SocketDataEvent> query_resp_event = event_gen.InitRecvEvent<kProtocolCQL>(
      CreateCQLEvent(cass::RespOp::kResult, kResultResp, stream));

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(query_req_event));
  source_->AcceptDataEvent(std::move(query_resp_event));

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = cql_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  EXPECT_THAT(record_batch, RecordBatchSizeIs(1));

  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kCQLReqOp]),
              ElementsAre(static_cast<int64_t>(cass::ReqOp::kQuery)));
  EXPECT_THAT(ToStringVector(record_batch[kCQLReqBody]), ElementsAre("SELECT * FROM system.peers"));

  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kCQLRespOp]),
              ElementsAre(static_cast<int64_t>(cass::RespOp::kResult)));
  EXPECT_THAT(ToStringVector(record_batch[kCQLRespBody]), ElementsAre(
                                                              R"(Response type = ROWS
Number of columns = 9
["peer","data_center","host_id","preferred_ip","rack","release_version","rpc_address","schema_version","tokens"]
Number of rows = 0)"));

  // In test environment, latencies are simply the number of packets in the response.
  // In this case 7 response packets: 1 header + 1 col defs + 1 EOF + 3 rows + 1 EOF.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kCQLLatency]), ElementsAre(1));
}

//-----------------------------------------------------------------------------
// HTTP2 specific tests
//-----------------------------------------------------------------------------

// A note about event generator clocks. Preferably, the test cases should all use MockClock,
// so we can verify latency calculations.
// UProbe-based HTTP2 capture, however, doesn't work with the MockClock because Cleanup() triggers
// and removes all events. For this reason we use RealClock for these tests.

TEST_F(SocketTraceConnectorTest, HTTP2ClientTest) {
  testing::EventGenerator event_gen(&mock_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, 7);

  source_->AcceptControlEvent(conn);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);
  EXPECT_EQ(record_batch[kHTTPReqMethodIdx]->Get<types::StringValue>(0), "post");
  EXPECT_EQ(record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(0), "/magic");
  EXPECT_EQ(record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(0), 200);
  EXPECT_THAT(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(0),
              ::testing::HasSubstr(R"(":method":"post")"));
  EXPECT_THAT(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(0),
              ::testing::HasSubstr(R"(":path":"/magic")"));
  EXPECT_THAT(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(0),
              ::testing::HasSubstr(R"(":status":"200")"));
}

// This test is like the previous one, but the read-write roles are reversed.
// It represents the other end of the connection.
TEST_F(SocketTraceConnectorTest, HTTP2ServerTest) {
  testing::EventGenerator event_gen(&mock_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, 8);

  source_->AcceptControlEvent(conn);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":path", "/magic"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Req"));
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventRead>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventWrite>());
  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);
  EXPECT_EQ(record_batch[kHTTPReqMethodIdx]->Get<types::StringValue>(0), "post");
  EXPECT_EQ(record_batch[kHTTPReqPathIdx]->Get<types::StringValue>(0), "/magic");
  EXPECT_EQ(record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(0), 200);
  EXPECT_THAT(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(0),
              ::testing::HasSubstr(R"(":method":"post")"));
  EXPECT_THAT(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(0),
              ::testing::HasSubstr(R"(":path":"/magic")"));
  EXPECT_THAT(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(0),
              ::testing::HasSubstr(R"(":status":"200")"));
}

// This test models capturing data mid-stream, where we may have missed the request entirely.
TEST_F(SocketTraceConnectorTest, HTTP2ResponseOnly) {
  testing::EventGenerator event_gen(&mock_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, 7);

  source_->AcceptControlEvent(conn);
  // Request missing to model mid-stream capture.
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());

  connector_->TransferData(ctx_.get());
  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  // TODO(oazizi): Someday we will need to capture response only streams properly.
  // In that case, we would expect certain values here.
  // EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "onse");
  // EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);
}

// This test models capturing data mid-stream, where we may have missed the request entirely.
TEST_F(SocketTraceConnectorTest, HTTP2SpanAcrossTransferData) {
  std::vector<TaggedRecordBatch> tablets;

  testing::EventGenerator event_gen(&mock_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, 7);

  source_->AcceptControlEvent(conn);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));

  connector_->TransferData(ctx_.get());

  // TransferData should not have pushed data to the tables, because HTTP2 stream is still active.
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  // TransferData should now have pushed data to the tables, because HTTP2 stream has ended.
  tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);
}

// This test models multiple streams back-to-back.
TEST_F(SocketTraceConnectorTest, HTTP2SequentialStreams) {
  testing::EventGenerator event_gen(&mock_clock_);

  std::vector<int> stream_ids = {7, 9, 11, 13};

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, stream_id);
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("uest"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id), /* end_stream */ true));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
    source_->AcceptHTTP2Data(
        frame_generator.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id)));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  }

  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(4));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response7");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request13");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response13");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 0);
}

// This test models multiple streams running in parallel.
TEST_F(SocketTraceConnectorTest, HTTP2ParallelStreams) {
  testing::EventGenerator event_gen(&mock_clock_);

  std::vector<uint32_t> stream_ids = {7, 9, 11, 13};
  std::map<uint32_t, testing::StreamEventGenerator> frame_generators;

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    frame_generators.insert(
        {stream_id, testing::StreamEventGenerator(&mock_clock_, conn.conn_id, stream_id)});
  }

  for (auto stream_id : stream_ids) {
    source_->AcceptHTTP2Header(
        frame_generators.at(stream_id).GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(
        frame_generators.at(stream_id).GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  }
  for (auto stream_id : stream_ids) {
    source_->AcceptHTTP2Header(
        frame_generators.at(stream_id).GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(
        frame_generators.at(stream_id).GenDataFrame<kDataFrameEventWrite>("Req"));
  }
  for (auto stream_id : stream_ids) {
    source_->AcceptHTTP2Data(
        frame_generators.at(stream_id).GenDataFrame<kDataFrameEventWrite>("uest"));
  }
  for (auto stream_id : stream_ids) {
    source_->AcceptHTTP2Data(frame_generators.at(stream_id).GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id), /* end_stream */ true));
    source_->AcceptHTTP2Data(
        frame_generators.at(stream_id).GenDataFrame<kDataFrameEventRead>("Resp"));
  }
  for (auto stream_id : stream_ids) {
    source_->AcceptHTTP2Data(
        frame_generators.at(stream_id).GenDataFrame<kDataFrameEventRead>("onse"));
    source_->AcceptHTTP2Data(frame_generators.at(stream_id).GenDataFrame<kDataFrameEventRead>(
        std::to_string(stream_id)));
  }
  for (auto stream_id : stream_ids) {
    source_->AcceptHTTP2Header(
        frame_generators.at(stream_id).GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(
        frame_generators.at(stream_id).GenEndStreamHeader<kHeaderEventRead>());
  }
  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(4));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response7");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request13");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response13");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 0);
}

// This test models one stream start and ending within the span of a larger stream.
// Random TransferData calls are interspersed just to make things more fun :)
TEST_F(SocketTraceConnectorTest, HTTP2StreamSandwich) {
  testing::EventGenerator event_gen(&mock_clock_);

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  uint32_t stream_id = 7;

  testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, stream_id);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  connector_->TransferData(ctx_.get());
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("uest"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>(
      std::to_string(stream_id), /* end_stream */ true));

  {
    uint32_t stream_id2 = 9;
    testing::StreamEventGenerator frame_generator2(&mock_clock_, conn.conn_id, stream_id2);
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>("Req"));
    connector_->TransferData(ctx_.get());
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>("uest"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id2), /* end_stream */ true));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventRead>("Resp"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventRead>("onse"));
    connector_->TransferData(ctx_.get());
    source_->AcceptHTTP2Data(
        frame_generator2.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id2)));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(frame_generator2.GenEndStreamHeader<kHeaderEventRead>());
  }

  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  connector_->TransferData(ctx_.get());
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id)));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());

  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  // Note that the records are pushed as soon as they complete. This is so
  // a long-running stream does not block other shorter streams from being recorded.
  // Notice, however, that this causes stream_id 9 to appear before stream_id 7.

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(2));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request9");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response9");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response7");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 0);
}

// This test models an old stream appearing slightly late.
TEST_F(SocketTraceConnectorTest, HTTP2StreamIDRace) {
  testing::EventGenerator event_gen(&mock_clock_);

  std::vector<int> stream_ids = {7, 9, 5, 11};

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, stream_id);
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("uest"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id), /* end_stream */ true));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
    source_->AcceptHTTP2Data(
        frame_generator.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id)));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  }

  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(4));

  // Note that results are sorted by time of request/response pair. See stream_ids vector above.

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response7");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request9");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response9");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(2), "Request5");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(2), "Response5");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(2), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request11");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response11");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 0);
}

// This test models an old stream appearing out-of-nowhere.
// Expectation is that we should be robust in such cases.
TEST_F(SocketTraceConnectorTest, HTTP2OldStream) {
  testing::EventGenerator event_gen(&mock_clock_);

  std::vector<int> stream_ids = {117, 119, 3, 121};

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    testing::StreamEventGenerator frame_generator(&mock_clock_, conn.conn_id, stream_id);
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("uest"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id), /* end_stream */ true));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
    source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
    source_->AcceptHTTP2Data(
        frame_generator.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id)));
    source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());

    connector_->TransferData(ctx_.get());
  }

  source_->AcceptControlEvent(event_gen.InitClose());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(4));

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request117");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response117");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request119");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response119");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(2), "Request3");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(2), "Response3");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(2), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request121");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response121");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 0);
}

}  // namespace stirling
}  // namespace px
