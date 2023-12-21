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

#include <memory>

#include <absl/functional/bind_front.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/shared/metadata/metadata.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

#include "src/common/testing/testing.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_connector_friend.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;

using ::testing::ElementsAre;

using ::px::stirling::testing::RecordBatchSizeIs;

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

const std::string_view kReq4 =
    "GET /test HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nUser-Agent: curl/7.68.0\r\nAccept: */*\r\n\r\n";

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

const std::string_view kResp4Header =
    "HTTP/1.0 200 OK\r\n"
    "Server: BaseHTTP/0.6 Python/3.10.1\r\n"
    "Date: Wed, 12 Jan 2022 17:37:21 GMT\r\n"
    "Content-type: application/json\r\n"
    "\r\n";

const std::string_view kResp4Body = "hello world";

//-----------------------------------------------------------------------------
// Tests
//-----------------------------------------------------------------------------

class SocketTraceConnectorTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kASID = 0;

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kCQLTableNum = SocketTraceConnector::kCQLTableNum;

  SocketTraceConnectorTest() : event_gen_(&mock_clock_) {}

  void SetUp() override {
    // Create and configure the connector.
    connector_ = SocketTraceConnectorFriend::Create("socket_trace_connector");
    connector_->set_data_tables(data_tables_.tables());
    source_ = dynamic_cast<SocketTraceConnectorFriend*>(connector_.get());
    ASSERT_NE(nullptr, source_);

    absl::flat_hash_set<md::UPID> upids = {md::UPID(kASID, kPID, kPIDStartTimeTicks)};
    ctx_ = std::make_unique<StandaloneContext>(upids);

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
  }

  DataTables data_tables_{SocketTraceConnector::kTables};

  DataTable* http_table_ = data_tables_[kHTTPTableNum];
  DataTable* cql_table_ = data_tables_[kCQLTableNum];

  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnectorFriend* source_ = nullptr;
  std::unique_ptr<StandaloneContext> ctx_;
  testing::MockClock mock_clock_;
  testing::EventGenerator event_gen_;
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

TEST_F(SocketTraceConnectorTest, Basic) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq3);
  std::unique_ptr<SocketDataEvent> event0_resp_json =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0_req));
  source_->AcceptDataEvent(std::move(event0_resp_json));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);

  ASSERT_THAT(records, RecordBatchSizeIs(1));
  EXPECT_THAT(ToStringVector(records[kHTTPReqBodyIdx]), ElementsAre("I have a message body"));
  EXPECT_THAT(ToStringVector(records[kHTTPRespBodyIdx]), ElementsAre("foo"));
}

TEST_F(SocketTraceConnectorTest, HTTPDelayedRespBody) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq4);
  std::unique_ptr<SocketDataEvent> event0_resp_header =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp4Header);
  std::unique_ptr<SocketDataEvent> event0_resp_body =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp4Body);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  connector_->TransferData(ctx_.get());

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0_req));
  source_->AcceptDataEvent(std::move(event0_resp_header));
  connector_->TransferData(ctx_.get());

  // Simulate a large delay between the resp header and body.
  mock_clock_.advance(2000000000);
  connector_->TransferData(ctx_.get());

  // Resp body.
  source_->AcceptDataEvent(std::move(event0_resp_body));
  connector_->TransferData(ctx_.get());

  // ConnClose.
  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);

  EXPECT_THAT(records, RecordBatchSizeIs(1));
  EXPECT_THAT(ToStringVector(records[kHTTPReqBodyIdx]), ElementsAre(""));
  EXPECT_THAT(ToStringVector(records[kHTTPRespBodyIdx]), ElementsAre("hello world"));
}

TEST_F(SocketTraceConnectorTest, HTTPContentType) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event0_resp_json =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> event1_resp_text =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kTextResp);
  std::unique_ptr<SocketDataEvent> event2_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> event2_resp_bin =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kAppOctetResp);
  std::unique_ptr<SocketDataEvent> event3_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event3_resp_json =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = event_gen_.InitClose();

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

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);

  EXPECT_THAT(records, RecordBatchSizeIs(4))
      << "The filter is changed to require 'application/json' in Content-Type header, "
         "and event_json Content-Type matches, and is selected";
  EXPECT_THAT(ToStringVector(records[kHTTPRespBodyIdx]),
              ElementsAre("foo", "bar", "<removed: non-text content-type>", "foo"));
  EXPECT_THAT(ToIntVector<types::Time64NSValue>(records[kHTTPTimeIdx]),
              ElementsAre(source_->ConvertToRealTime(3), source_->ConvertToRealTime(5),
                          source_->ConvertToRealTime(7), source_->ConvertToRealTime(9)));
}

TEST_F(SocketTraceConnectorTest, Truncation) {
  const std::string_view kResp0 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 26\r\n"
      "\r\n"
      "abcdefghijklmnopqrstuvwxyz";

  PX_SET_FOR_SCOPE(FLAGS_max_body_bytes, 5);

  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);

  EXPECT_THAT(records, RecordBatchSizeIs(1));
  EXPECT_THAT(ToStringVector(records[kHTTPRespBodyIdx]), ElementsAre("abcde... [TRUNCATED]"));
}

// Use CQL protocol to check sorting, because it supports parallel request-response streams.
TEST_F(SocketTraceConnectorTest, SortedByResponseTime) {
  using protocols::cass::ReqOp;
  using protocols::cass::RespOp;
  using protocols::cass::testutils::CreateCQLEmptyEvent;
  using protocols::cass::testutils::CreateCQLEvent;

  // A CQL request with CQL_VERSION=3.0.0.
  constexpr uint8_t kStartupReq1[] = {0x00, 0x01, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
                                      0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00,
                                      0x05, 0x33, 0x2e, 0x30, 0x2e, 0x30};

  // A CQL request with CQL_VERSION=3.0.1
  // Note that last byte is different than request above.
  constexpr uint8_t kStartupReq2[] = {0x00, 0x01, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
                                      0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00,
                                      0x05, 0x33, 0x2e, 0x30, 0x2e, 0x31};

  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req1 =
      event_gen_.InitSendEvent<kProtocolCQL>(CreateCQLEvent(ReqOp::kStartup, kStartupReq1, 1));
  std::unique_ptr<SocketDataEvent> req2 =
      event_gen_.InitSendEvent<kProtocolCQL>(CreateCQLEvent(ReqOp::kStartup, kStartupReq2, 2));
  std::unique_ptr<SocketDataEvent> resp2 =
      event_gen_.InitRecvEvent<kProtocolCQL>(CreateCQLEmptyEvent(RespOp::kReady, 2));
  std::unique_ptr<SocketDataEvent> resp1 =
      event_gen_.InitRecvEvent<kProtocolCQL>(CreateCQLEmptyEvent(RespOp::kReady, 1));
  struct socket_control_event_t close_event = event_gen_.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req1));
  source_->AcceptDataEvent(std::move(req2));
  source_->AcceptDataEvent(std::move(resp2));
  source_->AcceptDataEvent(std::move(resp1));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = cql_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);
  EXPECT_THAT(records, RecordBatchSizeIs(2));

  // Note that results are sorted by response time, not request time.
  EXPECT_THAT(ToStringVector(records[kCQLReqBody]),
              ElementsAre(R"({"CQL_VERSION":"3.0.1"})", R"({"CQL_VERSION":"3.0.0"})"));
}

TEST_F(SocketTraceConnectorTest, UPIDCheck) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event0_resp = event_gen_.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_req = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event1_resp = event_gen_.InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  // Registers a new connection.
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(std::move(event0_req)));
  source_->AcceptDataEvent(std::move(std::move(event0_resp)));
  source_->AcceptDataEvent(std::move(std::move(event1_req)));
  source_->AcceptDataEvent(std::move(std::move(event1_resp)));
  source_->AcceptControlEvent(close_event);

  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);

  ASSERT_THAT(records, RecordBatchSizeIs(2));

  for (int i = 0; i < 2; ++i) {
    auto val = records[kHTTPUPIDIdx]->Get<types::UInt128Value>(i);
    md::UPID upid(val.val);

    EXPECT_EQ(upid.pid(), kPID);
    EXPECT_EQ(upid.start_ts(), kPIDStartTimeTicks);
    EXPECT_EQ(upid.asid(), kASID);
  }
}

TEST_F(SocketTraceConnectorTest, AppendNonContiguousEvents) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> event2 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> event3 =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1.substr(0, kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event4 =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1.substr(kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event5 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> event6 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  std::vector<TaggedRecordBatch> tablets;

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0));
  source_->AcceptDataEvent(std::move(event2));
  source_->AcceptDataEvent(std::move(event5));
  source_->AcceptDataEvent(std::move(event1));
  source_->AcceptDataEvent(std::move(event4));
  source_->AcceptDataEvent(std::move(event6));
  connector_->TransferData(ctx_.get());
  connector_->TransferData(ctx_.get());

  tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);

  EXPECT_THAT(records, RecordBatchSizeIs(2));

  source_->AcceptDataEvent(std::move(event3));
  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get());

  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty()) << "Late events won't get processed.";
}

TEST_F(SocketTraceConnectorTest, NoEvents) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  std::vector<TaggedRecordBatch> tablets;

  // Check empty transfer.
  source_->AcceptControlEvent(conn);
  connector_->TransferData(ctx_.get());

  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  // Check empty transfer following a successful transfer.
  source_->AcceptDataEvent(std::move(event0));
  source_->AcceptDataEvent(std::move(event1));
  connector_->TransferData(ctx_.get());

  tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);
  EXPECT_THAT(records, RecordBatchSizeIs(1));

  // No new events, so expect no data.
  connector_->TransferData(ctx_.get());
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  // Close event shouldn't cause any new data either.
  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get());
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());
}

TEST_F(SocketTraceConnectorTest, RequestResponseMatching) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptControlEvent(close_event);
  connector_->TransferData(ctx_.get());

  std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);
  EXPECT_THAT(records, RecordBatchSizeIs(3));

  EXPECT_THAT(ToStringVector(records[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "doe"));
  EXPECT_THAT(ToStringVector(records[kHTTPReqMethodIdx]), ElementsAre("GET", "GET", "GET"));
  EXPECT_THAT(ToStringVector(records[kHTTPReqPathIdx]),
              ElementsAre("/index.html", "/data.html", "/logs.html"));
}

TEST_F(SocketTraceConnectorTest, MissingEventInStream) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  std::unique_ptr<SocketDataEvent> req_event3 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event3 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  // No Close event (connection still active).

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  PX_UNUSED(resp_event1);  // Missing event.
  source_->AcceptDataEvent(std::move(resp_event2));
  connector_->TransferData(ctx_.get());

  {
    std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);
    EXPECT_THAT(records, RecordBatchSizeIs(2));
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  // Processing of resp_event3 will result in one more record.
  source_->AcceptDataEvent(std::move(req_event3));
  source_->AcceptDataEvent(std::move(resp_event3));
  connector_->TransferData(ctx_.get());

  {
    std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(RecordBatch & records, tablets);
    EXPECT_THAT(records, RecordBatchSizeIs(1));
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInOrder) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(conn);

  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  connector_->TransferData(ctx_.get());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));

  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  connector_->TransferData(ctx_.get());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(close_event);
  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
    connector_->TransferData(ctx_.get());
  }

  EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  connector_->TransferData(ctx_.get());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupOutOfOrder) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptDataEvent(std::move(resp_event0));

  connector_->TransferData(ctx_.get());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  source_->AcceptControlEvent(close_event);
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(req_event2));

  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get());
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  connector_->TransferData(ctx_.get());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupMissingDataEvent) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req_event0 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> req_event3 = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  std::unique_ptr<SocketDataEvent> resp_event3 = event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  PX_UNUSED(resp_event1);  // Missing event.
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptControlEvent(close_event);

  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get());
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  connector_->TransferData(ctx_.get());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupOldGenerations) {
  struct socket_control_event_t conn0 = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> conn0_req_event = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> conn0_resp_event =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);
  struct socket_control_event_t conn0_close = event_gen_.InitClose();

  struct socket_control_event_t conn1 = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> conn1_req_event = event_gen_.InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> conn1_resp_event =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp1);
  struct socket_control_event_t conn1_close = event_gen_.InitClose();

  struct socket_control_event_t conn2 = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> conn2_req_event = event_gen_.InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> conn2_resp_event =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp2);
  struct socket_control_event_t conn2_close = event_gen_.InitClose();

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
  PX_UNUSED(conn0_close);  // Missing close event.
  PX_UNUSED(conn1_close);  // Missing close event.

  connector_->TransferData(ctx_.get());
  EXPECT_OK(source_->GetConnTracker(kPID, kFD));

  // TransferData results in countdown = kDeathCountdownIters for old generations.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get());
  }

  connector_->TransferData(ctx_.get());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupNoProtocol) {
  struct socket_control_event_t conn0 = event_gen_.InitConn();
  struct socket_control_event_t conn0_close = event_gen_.InitClose();

  source_->AcceptControlEvent(conn0);
  source_->AcceptControlEvent(conn0_close);

  // TransferData results in countdown = kDeathCountdownIters for old generations.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnTracker::kDeathCountdownIters - 1; ++i) {
    connector_->TransferData(ctx_.get());
    EXPECT_OK(source_->GetConnTracker(kPID, kFD));
  }

  connector_->TransferData(ctx_.get());
  EXPECT_NOT_OK(source_->GetConnTracker(kPID, kFD));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupCollecting) {
  // Create an event with family PX_AF_UNKNOWN so that tracker goes into collecting state.
  struct socket_control_event_t conn0 = event_gen_.InitConn();
  conn0.open.raddr.sa.sa_family = PX_AF_UNKNOWN;

  std::unique_ptr<SocketDataEvent> conn0_req_event =
      event_gen_.InitSendEvent<kProtocolHTTP>(kReq0.substr(0, 10));

  // Need an initial TransferData() to initialize the times.
  connector_->TransferData(ctx_.get());

  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));

  // After events arrive, we expect the tracker to be in collecting state
  {
    ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker, source_->GetConnTracker(kPID, kFD));
    ASSERT_EQ(tracker->state(), ConnTracker::State::kCollecting);
    ASSERT_GT(tracker->send_data().data_buffer().size(), 0);
  }

  connector_->TransferData(ctx_.get());

  // With default TransferData(), we expect the data to be retained.
  {
    ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker, source_->GetConnTracker(kPID, kFD));
    ASSERT_EQ(tracker->state(), ConnTracker::State::kCollecting);
    ASSERT_GT(tracker->send_data().data_buffer().size(), 0);
  }

  // Now set retention size to 0 and expect the collecting tracker to be cleaned-up.
  PX_SET_FOR_SCOPE(FLAGS_datastream_buffer_retention_size, 0);
  connector_->TransferData(ctx_.get());

  {
    ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker, source_->GetConnTracker(kPID, kFD));
    ASSERT_EQ(tracker->state(), ConnTracker::State::kCollecting);
    ASSERT_EQ(tracker->send_data().data_buffer().size(), 0);
  }
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInactiveDead) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_check_proc_for_conn_close, true);

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
    connector_->TransferData(ctx_.get());
  }

  // Connection should have been marked as idle by now,
  // and a check of /proc/<pid>/<fd> will trigger MarkForDeath().

  EXPECT_NOT_OK(source_->GetConnTracker(impossible_pid, 1));
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInactiveAlive) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_check_proc_for_conn_close, true);
  std::chrono::seconds kInactivityDuration(1);
  ConnTracker::set_inactivity_duration(kInactivityDuration);

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

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));

  for (int i = 0; i < 100; ++i) {
    connector_->TransferData(ctx_.get());
    EXPECT_OK(source_->GetConnTracker(real_pid, real_fd));
  }

  // Advance the time by the inactivity duration.
  mock_clock_.advance(
      std::chrono::duration_cast<std::chrono::nanoseconds>(kInactivityDuration).count());

  // Connection should be timed out by next TransferData,
  // which should also cause events to be flushed, but the connection is still alive.
  connector_->TransferData(ctx_.get());
  EXPECT_OK(source_->GetConnTracker(real_pid, real_fd));

  // Should not have transferred any data.
  tablets = http_table_->ConsumeRecords();
  ASSERT_TRUE(tablets.empty());

  // Events should have been flushed.
  ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker, source_->GetConnTracker(real_pid, real_fd));
  EXPECT_TRUE((tracker->recv_data().Empty<http::stream_id_t, http::Message>()));
  EXPECT_TRUE((tracker->send_data().Empty<http::stream_id_t, http::Message>()));
}

TEST_F(SocketTraceConnectorTest, TrackedUPIDTransfersData) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_untracked_upid_threshold_seconds, 1);

  // By default, events come from a pid that is in the context.
  struct socket_control_event_t conn0 = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> conn0_req_event = event_gen_.InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> conn0_resp_event =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kResp0);

  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));
  source_->AcceptDataEvent(std::move(conn0_resp_event));

  connector_->TransferData(ctx_.get());

  ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker, source_->GetConnTracker(kPID, kFD));
  EXPECT_TRUE(tracker->is_tracked_upid());
  EXPECT_EQ(tracker->state(), ConnTracker::State::kTransferring);

  // Should have transferred the data.
  std::vector<TaggedRecordBatch> tablets;
  tablets = http_table_->ConsumeRecords();

  ASSERT_FALSE(tablets.empty());
  RecordBatch& record_batch = tablets[0].records;
  EXPECT_THAT(record_batch, RecordBatchSizeIs(1));
}

TEST_F(SocketTraceConnectorTest, UntrackedUPIDDoesNotTransferData) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_untracked_upid_threshold_seconds, 1);

  // Choose an event generator for a pid that is not in the context.
  const uint32_t kNonContextPID = 1;
  const uint32_t kNonContextFD = 37;
  testing::EventGenerator event_gen(&mock_clock_, kNonContextPID, kNonContextFD);

  // Simulated events.
  source_->AcceptControlEvent(event_gen.InitConn());
  source_->AcceptDataEvent(event_gen.InitSendEvent<kProtocolHTTP>(kReq0));
  source_->AcceptDataEvent(event_gen.InitRecvEvent<kProtocolHTTP>(kResp0));

  connector_->TransferData(ctx_.get());

  ASSERT_OK_AND_ASSIGN(const ConnTracker* tracker,
                       source_->GetConnTracker(kNonContextPID, kNonContextFD));
  EXPECT_FALSE(tracker->is_tracked_upid());
  EXPECT_EQ(tracker->state(), ConnTracker::State::kTransferring);

  // Record should be transferred.
  // TODO(oazizi): Change behavior that the tracker stays in the collecting state, and doesn't
  // transfer data.
  {
    std::vector<TaggedRecordBatch> tablets = http_table_->ConsumeRecords();

    ASSERT_FALSE(tablets.empty());
    RecordBatch& record_batch = tablets[0].records;
    EXPECT_THAT(record_batch, RecordBatchSizeIs(1));
  }

  // After some more time, the tracker should be disabled.
  mock_clock_.advance(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2)).count());

  source_->AcceptDataEvent(event_gen.InitSendEvent<kProtocolHTTP>(kReq0));
  source_->AcceptDataEvent(event_gen.InitRecvEvent<kProtocolHTTP>(kResp0));

  connector_->TransferData(ctx_.get());

  EXPECT_FALSE(tracker->is_tracked_upid());
  EXPECT_EQ(tracker->state(), ConnTracker::State::kDisabled);

  // Should not transfer any data anymore.
  ASSERT_TRUE(http_table_->ConsumeRecords().empty());
}

}  // namespace stirling
}  // namespace px
