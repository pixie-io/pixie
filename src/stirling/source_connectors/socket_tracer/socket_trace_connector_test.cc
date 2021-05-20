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

#include <sys/socket.h>
#include <memory>

#include "src/shared/metadata/metadata.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

#include "src/common/testing/testing.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"
#include "src/stirling/source_connectors/socket_tracer/testing/http2_stream_generator.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;
namespace mysql = protocols::mysql;
namespace cass = protocols::cass;

using ::testing::Each;
using ::testing::ElementsAre;

using ::px::stirling::testing::ColWrapperSizeIs;

using ::px::stirling::testing::kFD;
using ::px::stirling::testing::kPID;
using ::px::stirling::testing::kPIDStartTimeTicks;

using RecordBatch = types::ColumnWrapperRecordBatch;

//-----------------------------------------------------------------------------
// Test data
//-----------------------------------------------------------------------------

const std::string_view kReq0 =
    "GET /index.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

const std::string_view kReq1 =
    "GET /data.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

const std::string_view kReq2 =
    "GET /logs.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

const std::string_view kReq3 =
    "POST /logs.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "Content-Length: 21\r\n"
    "\r\n"
    "I have a message body";

const std::string_view kJSONResp =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "foo";

const std::string_view kTextResp =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "bar";

const std::string_view kAppOctetResp =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/octet-stream\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "\x01\x23\x45";

const std::string_view kResp0 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: json\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "foo";

const std::string_view kResp1 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: json\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "bar";

const std::string_view kResp2 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: json\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "doe";

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
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    source_ = dynamic_cast<SocketTraceConnector*>(connector_.get());
    ASSERT_NE(nullptr, source_);

    ctx_ = std::make_unique<StandaloneContext>();

    // Set the CIDR for HTTP2ServerTest, which would otherwise not output any data,
    // because it would think the server is in the cluster.
    PL_CHECK_OK(ctx_->SetClusterCIDR("1.2.3.4/32"));

    // Because some tests change the inactivity duration, make sure to reset it here for each test.
    ConnTracker::set_inactivity_duration(ConnTracker::kDefaultInactivityDuration);

    FLAGS_stirling_check_proc_for_conn_close = false;
    FLAGS_stirling_conn_stats_sampling_ratio = 1;

    data_tables_ = std::make_unique<testing::DataTables>(SocketTraceConnector::kTables);

    // For convenience.
    http_table_ = (*data_tables_)[SocketTraceConnector::kHTTPTableNum];
    mysql_table_ = (*data_tables_)[SocketTraceConnector::kMySQLTableNum];
    cql_table_ = (*data_tables_)[SocketTraceConnector::kCQLTableNum];
  }

  std::unique_ptr<testing::DataTables> data_tables_;
  DataTable* http_table_;
  DataTable* mysql_table_;
  DataTable* cql_table_;

  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnector* source_ = nullptr;
  std::unique_ptr<StandaloneContext> ctx_;
  testing::MockClock mock_clock_;
  testing::RealClock real_clock_;

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

TEST_F(SocketTraceConnectorTest, HTTPBasic) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq3);
  std::unique_ptr<SocketDataEvent> event0_resp_json =
      event_gen.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = event_gen.InitClose();

  EXPECT_NE(0, source_->ClockRealTimeOffset());

  // Registers a new connection.
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0_req));
  source_->AcceptDataEvent(std::move(event0_resp_json));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;

  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqBodyIdx]), ElementsAre("I have a message body"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo"));
}

TEST_F(SocketTraceConnectorTest, HTTPContentType) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event0_resp_json =
      event_gen.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> event1_resp_text =
      event_gen.InitRecvEvent<kProtocolHTTP>(kTextResp);
  std::unique_ptr<SocketDataEvent> event2_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> event2_resp_bin =
      event_gen.InitRecvEvent<kProtocolHTTP>(kAppOctetResp);
  std::unique_ptr<SocketDataEvent> event3_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event3_resp_json =
      event_gen.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = event_gen.InitClose();

  EXPECT_NE(0, source_->ClockRealTimeOffset());

  // Registers a new connection.
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0_req));
  source_->AcceptDataEvent(std::move(event0_resp_json));
  source_->AcceptDataEvent(std::move(event1_req));
  source_->AcceptDataEvent(std::move(event1_resp_text));
  source_->AcceptDataEvent(std::move(event2_req));
  source_->AcceptDataEvent(std::move(event2_resp_bin));
  source_->AcceptDataEvent(std::move(event3_req));
  source_->AcceptDataEvent(std::move(event3_resp_json));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;

  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(4)))
      << "The filter is changed to require 'application/json' in Content-Type header, "
         "and event_json Content-Type matches, and is selected";
  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]),
              ElementsAre("foo", "bar", "<removed: non-text content-type>", "foo"));
  EXPECT_THAT(ToIntVector<types::Time64NSValue>(record_batch[kHTTPTimeIdx]),
              ElementsAre(3 + source_->ClockRealTimeOffset(), 5 + source_->ClockRealTimeOffset(),
                          7 + source_->ClockRealTimeOffset(), 9 + source_->ClockRealTimeOffset()));
}

// Use CQL protocol to check sorting, because it supports parallel request-response streams.
TEST_F(SocketTraceConnectorTest, SortedByResponseTime) {
  using cass::testutils::CreateCQLEmptyEvent;
  using cass::testutils::CreateCQLEvent;

  testing::EventGenerator event_gen(&mock_clock_);

  // A CQL request with CQL_VERSION=3.0.0.
  constexpr uint8_t kStartupReq1[] = {0x00, 0x01, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
                                      0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00,
                                      0x05, 0x33, 0x2e, 0x30, 0x2e, 0x30};

  // A CQL request with CQL_VERSION=3.0.1
  // Note that last byte is different than request above.
  constexpr uint8_t kStartupReq2[] = {0x00, 0x01, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
                                      0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00,
                                      0x05, 0x33, 0x2e, 0x30, 0x2e, 0x31};

  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req1 =
      event_gen.InitSendEvent<kProtocolCQL>(CreateCQLEvent(cass::ReqOp::kStartup, kStartupReq1, 1));
  std::unique_ptr<SocketDataEvent> req2 =
      event_gen.InitSendEvent<kProtocolCQL>(CreateCQLEvent(cass::ReqOp::kStartup, kStartupReq2, 2));
  std::unique_ptr<SocketDataEvent> resp2 =
      event_gen.InitRecvEvent<kProtocolCQL>(CreateCQLEmptyEvent(cass::RespOp::kReady, 2));
  std::unique_ptr<SocketDataEvent> resp1 =
      event_gen.InitRecvEvent<kProtocolCQL>(CreateCQLEmptyEvent(cass::RespOp::kReady, 1));
  struct socket_control_event_t close_event = event_gen.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req1));
  source_->AcceptDataEvent(std::move(req2));
  source_->AcceptDataEvent(std::move(resp2));
  source_->AcceptDataEvent(std::move(resp1));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = cql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(2)));

  // Note that results are sorted by response time, not request time.
  EXPECT_THAT(ToStringVector(record_batch[kCQLReqBody]),
              ElementsAre(R"({"CQL_VERSION":"3.0.1"})", R"({"CQL_VERSION":"3.0.0"})"));
}

TEST_F(SocketTraceConnectorTest, UPIDCheck) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event0_resp = event_gen.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_req = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event1_resp = event_gen.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = event_gen.InitClose();

  // Registers a new connection.
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(std::move(event0_req)));
  source_->AcceptDataEvent(std::move(std::move(event0_resp)));
  source_->AcceptDataEvent(std::move(std::move(event1_req)));
  source_->AcceptDataEvent(std::move(std::move(event1_resp)));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;

  for (const auto& column : record_batch) {
    ASSERT_EQ(2, column->Size());
  }

  for (int i = 0; i < 2; ++i) {
    auto val = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(i);
    md::UPID upid(val.val);

    EXPECT_EQ(upid.pid(), kPID);
    EXPECT_EQ(upid.start_ts(), kPIDStartTimeTicks);
    EXPECT_EQ(upid.asid(), kASID);
  }
}

TEST_F(SocketTraceConnectorTest, AppendNonContiguousEvents) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> event2 = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> event3 =
      event_gen.InitRecvEvent<kProtocolHTTP>(kResp1.substr(0, kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event4 =
      event_gen.InitRecvEvent<kProtocolHTTP>(kResp1.substr(kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event5 = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> event6 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  std::vector<TaggedRecordBatch> tablets;
  RecordBatch record_batch;

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0));
  source_->AcceptDataEvent(std::move(event2));
  source_->AcceptDataEvent(std::move(event5));
  source_->AcceptDataEvent(std::move(event1));
  source_->AcceptDataEvent(std::move(event4));
  source_->AcceptDataEvent(std::move(event6));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  connector_->TransferData(ctx_.get(), data_tables_->tables());

  tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_EQ(2, record_batch[0]->Size());

  source_->AcceptDataEvent(std::move(event3));
  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get(), data_tables_->tables());

  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty()) << "Late events won't get processed.";
}

TEST_F(SocketTraceConnectorTest, NoEvents) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  struct socket_control_event_t close_event = event_gen.InitClose();

  std::vector<TaggedRecordBatch> tablets;
  RecordBatch record_batch;

  source_->AcceptControlEvent(conn);

  // Check empty transfer.
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  // Check empty transfer following a successful transfer.
  source_->AcceptDataEvent(std::move(event0));
  source_->AcceptDataEvent(std::move(event1));

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_EQ(1, record_batch[0]->Size());

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());
}

TEST_F(SocketTraceConnectorTest, RequestResponseMatching) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  EXPECT_EQ(3, record_batch[0]->Size());

  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "doe"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqMethodIdx]), ElementsAre("GET", "GET", "GET"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqPathIdx]),
              ElementsAre("/index.html", "/data.html", "/logs.html"));
}

TEST_F(SocketTraceConnectorTest, MissingEventInStream) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  std::unique_ptr<SocketDataEvent> req_event3 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event3 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  // No Close event (connection still active).

  std::vector<TaggedRecordBatch> tablets;
  RecordBatch record_batch;

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  PL_UNUSED(resp_event1);  // Missing event.
  source_->AcceptDataEvent(std::move(resp_event2));

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_EQ(2, record_batch[0]->Size());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptDataEvent(std::move(req_event3));
  source_->AcceptDataEvent(std::move(resp_event3));

  // Processing of resp_event3 will result in one more record.
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_EQ(1, record_batch[0]->Size());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInOrder) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(conn);

  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));

  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(close_event);
  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
    connector_->TransferData(ctx_.get(), data_tables_->tables());
  }

  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupOutOfOrder) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptDataEvent(std::move(resp_event0));

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(close_event);
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(req_event2));

  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get(), data_tables_->tables());
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupMissingDataEvent) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> req_event3 = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  std::unique_ptr<SocketDataEvent> resp_event3 = event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  PL_UNUSED(resp_event1);  // Missing event.
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptControlEvent(close_event);

  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get(), data_tables_->tables());
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupOldGenerations) {
  testing::EventGenerator event_gen(&mock_clock_);

  struct socket_control_event_t conn0 = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> conn0_req_event = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> conn0_resp_event =
      event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);
  struct socket_control_event_t conn0_close = event_gen.InitClose();

  struct socket_control_event_t conn1 = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> conn1_req_event = event_gen.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> conn1_resp_event =
      event_gen.InitRecvEvent<kProtocolHTTP>(kResp1);
  struct socket_control_event_t conn1_close = event_gen.InitClose();

  struct socket_control_event_t conn2 = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> conn2_req_event = event_gen.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> conn2_resp_event =
      event_gen.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t conn2_close = event_gen.InitClose();

  // Simulating scrambled order due to perf buffer, with a couple missing events.
  source_->AcceptDataEvent(std::move(conn0_req_event));
  source_->AcceptControlEvent(conn1);
  source_->AcceptControlEvent(conn2_close);
  source_->AcceptDataEvent(std::move(conn0_resp_event));
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn2_req_event));
  source_->AcceptDataEvent(std::move(conn1_resp_event));
  source_->AcceptDataEvent(std::move(conn1_req_event));
  source_->AcceptControlEvent(conn2);
  source_->AcceptDataEvent(std::move(conn2_resp_event));
  PL_UNUSED(conn0_close);  // Missing close event.
  PL_UNUSED(conn1_close);  // Missing close event.

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  // TransferData results in countdown = kDeathCountdownIters for old generations.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get(), data_tables_->tables());
  }

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupNoProtocol) {
  testing::EventGenerator event_gen(&mock_clock_);
  struct socket_control_event_t conn0 = event_gen.InitConn();
  struct socket_control_event_t conn0_close = event_gen.InitClose();

  source_->AcceptControlEvent(conn0);
  source_->AcceptControlEvent(conn0_close);

  // TransferData results in countdown = kDeathCountdownIters for old generations.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get(), data_tables_->tables());
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInactiveDead) {
  FLAGS_stirling_check_proc_for_conn_close = true;

  // Inactive dead connections are determined by checking the /proc filesystem.
  // Here we create a PID that is a valid number, but non-existent on any Linux system.
  // Note that max PID bits in Linux is 22 bits.
  const uint32_t impossible_pid = 1 << 23;

  testing::EventGenerator event_gen(&mock_clock_, impossible_pid, 1);
  struct socket_control_event_t conn0 = event_gen.InitConn();

  std::unique_ptr<SocketDataEvent> conn0_req_event = event_gen.InitSendEvent<kProtocolHTTP>(kReq0);

  std::unique_ptr<SocketDataEvent> conn0_resp_event =
      event_gen.InitRecvEvent<kProtocolHTTP>(kResp0);

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));
  source_->AcceptDataEvent(std::move(conn0_resp_event));

  // Note that close event was not recorded, so this connection remains open.

  // Start with an active connection.
  EXPECT_OK(source_->GetConnTracker(impossible_pid, 1));

  // A bunch of iterations to trigger the idleness check.
  for (int i = 0; i < 100; ++i) {
    connector_->TransferData(ctx_.get(), data_tables_->tables());
  }

  // Connection should have been marked as idle by now,
  // and a check of /proc/<pid>/<fd> will trigger MarkForDeath().

  EXPECT_NOT_OK(source_->GetConnTracker(impossible_pid, 1));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInactiveAlive) {
  FLAGS_stirling_check_proc_for_conn_close = true;
  ConnTracker::set_inactivity_duration(std::chrono::seconds(1));

  // Inactive alive connections are determined by checking the /proc filesystem.
  // Here we create a PID that is a real PID, by using the test process itself.
  // And we create a real FD, by using FD 1, which is stdout.

  uint32_t real_pid = getpid();
  uint32_t real_fd = 1;

  testing::EventGenerator event_gen(&mock_clock_, real_pid, real_fd);
  struct socket_control_event_t conn0 = event_gen.InitConn();

  // An incomplete message means it shouldn't be parseable (we don't want TransferData to succeed).
  std::unique_ptr<SocketDataEvent> conn0_req_event =
      event_gen.InitSendEvent<kProtocolHTTP>("GET /index.html HTTP/1.1\r\n");

  std::vector<TaggedRecordBatch> tablets;
  RecordBatch record_batch;

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));

  for (int i = 0; i < 100; ++i) {
    connector_->TransferData(ctx_.get(), data_tables_->tables());
    EXPECT_OK(source_->GetConnTracker(real_pid, real_fd));
  }

  ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker, source_->GetConnTracker(real_pid, real_fd));

  sleep(2);

  // Connection should be timed out by next TransferData,
  // which should also cause events to be flushed, but the connection is still alive.

  EXPECT_OK(source_->GetConnTracker(real_pid, real_fd));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  EXPECT_OK(source_->GetConnTracker(real_pid, real_fd));

  // Should not have transferred any data.
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  // Events should have been flushed.
  EXPECT_TRUE(tracker->recv_data().Empty<http::Message>());
  EXPECT_TRUE(tracker->send_data().Empty<http::Message>());
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
  RecordBatch record_batch;

  connector_->TransferData(ctx_.get(), data_tables_->tables());
  tablets = mysql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(2)));

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
  // StmtPrepare resp has 7 response packets: 1 header + 2 col defs + 1 EOF + 2 param defs + 1 EOF.
  // StmtExecute resp has 7 response packets: 1 header + 2 col defs + 1 EOF + 2 rows + 1 EOF.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(7, 7));

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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  tablets = mysql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(2)));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]),
              ElementsAre("", "Execute stmt_id=2."));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]),
              ElementsAre("", "This is an error."));
  // In test environment, latencies are simply the number of packets in the response.
  // StmtClose resp has 0 response packets.
  // StmtExecute resp has 1 response packet: 1 error.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(0, 1));
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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(1)));

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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(9)));

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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = mysql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
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
  using cass::testutils::kCQLLatencyIdx;
  using cass::testutils::kCQLReqBodyIdx;
  using cass::testutils::kCQLReqOpIdx;
  using cass::testutils::kCQLRespBodyIdx;
  using cass::testutils::kCQLRespOpIdx;

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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = cql_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(1)));

  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kCQLReqOpIdx]),
              ElementsAre(static_cast<int64_t>(cass::ReqOp::kQuery)));
  EXPECT_THAT(ToStringVector(record_batch[kCQLReqBodyIdx]),
              ElementsAre("SELECT * FROM system.peers"));

  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kCQLRespOpIdx]),
              ElementsAre(static_cast<int64_t>(cass::RespOp::kResult)));
  EXPECT_THAT(ToStringVector(record_batch[kCQLRespBodyIdx]), ElementsAre(
                                                                 R"(Response type = ROWS
Number of columns = 9
["peer","data_center","host_id","preferred_ip","rack","release_version","rpc_address","schema_version","tokens"]
Number of rows = 0)"));

  // In test environment, latencies are simply the number of packets in the response.
  // In this case 7 response packets: 1 header + 1 col defs + 1 EOF + 3 rows + 1 EOF.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kCQLLatencyIdx]), ElementsAre(1));
}

//-----------------------------------------------------------------------------
// HTTP2 specific tests
//-----------------------------------------------------------------------------

// A note about event generator clocks. Preferably, the test cases should all use MockClock,
// so we can verify latency calculations.
// UProbe-based HTTP2 capture, however, doesn't work with the MockClock because Cleanup() triggers
// and removes all events. For this reason we use RealClock for these tests.

TEST_F(SocketTraceConnectorTest, HTTP2ClientTest) {
  testing::EventGenerator event_gen(&real_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, 7);

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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
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
  testing::EventGenerator event_gen(&real_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, 8);

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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
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

// This test models capturing data mid-stream, where we may have missed the request headers.
TEST_F(SocketTraceConnectorTest, HTTP2PartialStream) {
  testing::EventGenerator event_gen(&real_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, 7);

  source_->AcceptControlEvent(conn);
  // Request headers are missing to model mid-stream capture.
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "uest");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);
}

// This test models capturing data mid-stream, where we may have missed the request entirely.
TEST_F(SocketTraceConnectorTest, HTTP2ResponseOnly) {
  testing::EventGenerator event_gen(&real_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, 7);

  source_->AcceptControlEvent(conn);
  // Request missing to model mid-stream capture.
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());

  connector_->TransferData(ctx_.get(), data_tables_->tables());
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
  RecordBatch record_batch;

  testing::EventGenerator event_gen(&real_clock_);

  auto conn = event_gen.InitConn();

  testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, 7);

  source_->AcceptControlEvent(conn);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  // TransferData should not have pushed data to the tables, because HTTP2 stream is still active.
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  // TransferData should now have pushed data to the tables, because HTTP2 stream has ended.
  tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);
}

// This test models multiple streams back-to-back.
TEST_F(SocketTraceConnectorTest, HTTP2SequentialStreams) {
  testing::EventGenerator event_gen(&real_clock_);

  std::vector<int> stream_ids = {7, 9, 11, 13};

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, stream_id);
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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response7");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request13");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response13");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 0);
}

// This test models multiple streams running in parallel.
TEST_F(SocketTraceConnectorTest, HTTP2ParallelStreams) {
  testing::EventGenerator event_gen(&real_clock_);

  std::vector<uint32_t> stream_ids = {7, 9, 11, 13};
  std::map<uint32_t, testing::StreamEventGenerator> frame_generators;

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    frame_generators.insert(
        {stream_id, testing::StreamEventGenerator(&real_clock_, conn.conn_id, stream_id)});
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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));
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
  testing::EventGenerator event_gen(&real_clock_);

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  uint32_t stream_id = 7;

  testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, stream_id);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("uest"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>(
      std::to_string(stream_id), /* end_stream */ true));

  {
    uint32_t stream_id2 = 9;
    testing::StreamEventGenerator frame_generator2(&real_clock_, conn.conn_id, stream_id2);
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>("Req"));
    connector_->TransferData(ctx_.get(), data_tables_->tables());
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>("uest"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id2), /* end_stream */ true));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventRead>("Resp"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventRead>("onse"));
    connector_->TransferData(ctx_.get(), data_tables_->tables());
    source_->AcceptHTTP2Data(
        frame_generator2.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id2)));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(frame_generator2.GenEndStreamHeader<kHeaderEventRead>());
  }

  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  connector_->TransferData(ctx_.get(), data_tables_->tables());
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id)));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());

  source_->AcceptControlEvent(event_gen.InitClose());

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  // Note that the records are pushed as soon as they complete. This is so
  // a long-running stream does not block other shorter streams from being recorded.
  // Notice, however, that this causes stream_id 9 to appear before stream_id 7.

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(2)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request9");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response9");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 0);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response7");
  EXPECT_GT(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 0);
}

// This test models an old stream appearing slightly late.
TEST_F(SocketTraceConnectorTest, HTTP2StreamIDRace) {
  testing::EventGenerator event_gen(&real_clock_);

  std::vector<int> stream_ids = {7, 9, 5, 11};

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, stream_id);
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

  connector_->TransferData(ctx_.get(), data_tables_->tables());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));

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
  testing::EventGenerator event_gen(&real_clock_);

  std::vector<int> stream_ids = {117, 119, 3, 121};

  auto conn = event_gen.InitConn();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    testing::StreamEventGenerator frame_generator(&real_clock_, conn.conn_id, stream_id);
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

    connector_->TransferData(ctx_.get(), data_tables_->tables());
  }

  source_->AcceptControlEvent(event_gen.InitClose());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  RecordBatch record_batch = tablets[0].records;
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));

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
