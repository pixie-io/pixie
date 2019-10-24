#include "src/stirling/socket_trace_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/socket.h>
#include <memory>

#include "src/shared/metadata/metadata.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"

#include "src/stirling/data_table.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ::testing::ElementsAre;
using RecordBatch = types::ColumnWrapperRecordBatch;

class SocketTraceConnectorTest : public testing::EventsFixture {
 protected:
  static constexpr uint32_t kASID = 1;

  void SetUp() override {
    testing::EventsFixture::SetUp();

    // Create and configure the connector.
    FLAGS_stirling_enable_http_tracing = true;
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    source_ = dynamic_cast<SocketTraceConnector*>(connector_.get());
    ASSERT_NE(nullptr, source_);

    auto agent_metadata_state = std::make_shared<md::AgentMetadataState>(kASID);
    ctx_ = std::make_unique<ConnectorContext>(agent_metadata_state);

    // Because some tests change the inactivity duration, make sure to reset it here for each test.
    ConnectionTracker::SetInactivityDuration(ConnectionTracker::kDefaultInactivityDuration);
    InitMySQLData();
  }

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;

  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnector* source_ = nullptr;
  std::unique_ptr<ConnectorContext> ctx_;

  const std::string kReq0 =
      "GET /index.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kReq1 =
      "GET /data.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kReq2 =
      "GET /logs.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kJSONResp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";

  const std::string kTextResp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";

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

  // MySQL test inputs
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;
  static constexpr int kMySQLReqBodyIdx = kMySQLTable.ColIndex("req_body");
  static constexpr int kMySQLRespBodyIdx = kMySQLTable.ColIndex("resp_body");
  static constexpr int kMySQLLatencyIdx = kMySQLTable.ColIndex("latency_ns");

  std::string mysql_stmt_prepare_req;
  std::vector<std::string> mysql_stmt_prepare_resp;
  std::string mysql_stmt_execute_req;
  std::vector<std::string> mysql_stmt_execute_resp;
  std::string mysql_stmt_close_req;
  std::string mysql_err_resp;

  std::string mysql_query_req;
  std::vector<std::string> mysql_query_resp;

  void InitMySQLData() {
    mysql_stmt_prepare_req = mysql::testutils::GenRawPacket(mysql::testutils::GenStringRequest(
        mysql::testdata::kStmtPrepareRequest, mysql::MySQLEventType::kStmtPrepare));

    std::deque<mysql::Packet> prepare_packets =
        mysql::testutils::GenStmtPrepareOKResponse(mysql::testdata::kStmtPrepareResponse);
    for (const auto& prepare_packet : prepare_packets) {
      mysql_stmt_prepare_resp.push_back(mysql::testutils::GenRawPacket(prepare_packet));
    }

    mysql_stmt_execute_req = mysql::testutils::GenRawPacket(
        mysql::testutils::GenStmtExecuteRequest(mysql::testdata::kStmtExecuteRequest));

    std::deque<mysql::Packet> execute_packets =
        mysql::testutils::GenResultset(mysql::testdata::kStmtExecuteResultset);
    for (const auto& execute_packet : execute_packets) {
      mysql_stmt_execute_resp.push_back(mysql::testutils::GenRawPacket(execute_packet));
    }

    mysql::ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
    mysql_err_resp = mysql::testutils::GenRawPacket(mysql::testutils::GenErr(1, err_resp));

    mysql_stmt_close_req = mysql::testutils::GenRawPacket(
        mysql::testutils::GenStmtCloseRequest(mysql::testdata::kStmtCloseRequest));

    mysql_query_req = mysql::testutils::GenRawPacket(mysql::testutils::GenStringRequest(
        mysql::testdata::kQueryRequest, mysql::MySQLEventType::kQuery));
    std::deque<mysql::Packet> query_packets =
        mysql::testutils::GenResultset(mysql::testdata::kQueryResultset);
    for (const auto& query_packet : query_packets) {
      mysql_query_resp.push_back(mysql::testutils::GenRawPacket(query_packet));
    }
  }
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

TEST_F(SocketTraceConnectorTest, End2End) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0_json = InitRecvEvent(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_text = InitRecvEvent(kTextResp);
  std::unique_ptr<SocketDataEvent> event2_text = InitRecvEvent(kTextResp);
  std::unique_ptr<SocketDataEvent> event3_json = InitRecvEvent(kJSONResp);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  EXPECT_NE(0, source_->ClockRealTimeOffset());

  // Registers a new connection.
  source_->AcceptControlEvent(conn);

  ASSERT_THAT(source_->NumActiveConnections(), 1);

  conn_id_t search_conn_id;
  search_conn_id.pid = kPID;
  search_conn_id.pid_start_time_ticks = kPIDStartTimeTicks;
  search_conn_id.fd = kFD;
  search_conn_id.generation = 1;
  const ConnectionTracker* tracker = source_->GetConnectionTracker(search_conn_id);
  ASSERT_NE(nullptr, tracker);
  EXPECT_EQ(1, tracker->conn().timestamp_ns);

  // AcceptDataEvent(std::move() puts data into the internal buffer of SocketTraceConnector. And
  // th)en TransferData() polls perf buffer, which is no-op because we did not initialize probes,
  // and the data in the internal buffer is being processed and filtered.
  source_->AcceptDataEvent(std::move(event0_json));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_json Content-Type does have 'json', and will be selected by the default filter";
  }

  source_->AcceptDataEvent(std::move(event1_text));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_text Content-Type has no 'json', and won't be selected by the default filter";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "text/plain"}},
      {{"Content-Encoding", "gzip"}},
  });
  source_->AcceptDataEvent(std::move(event2_text));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(2, column->Size())
        << "The filter is changed to require 'text/plain' in Content-Type header, "
           "and event_json Content-Type does not match, and won't be selected";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "application/json"}},
      {{"Content-Encoding", "gzip"}},
  });
  source_->AcceptDataEvent(std::move(event3_json));
  source_->AcceptControlEvent(close_event);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(3, column->Size())
        << "The filter is changed to require 'application/json' in Content-Type header, "
           "and event_json Content-Type matches, and is selected";
  }
  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "foo"));
  EXPECT_THAT(ToIntVector<types::Time64NSValue>(record_batch[kHTTPTimeIdx]), ElementsAre(2, 4, 5));
}

TEST_F(SocketTraceConnectorTest, UPIDCheck) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0_json = InitRecvEvent(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_json = InitRecvEvent(kJSONResp);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  // Registers a new connection.
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(std::move(event0_json)));
  source_->AcceptDataEvent(std::move(std::move(event1_json)));
  source_->AcceptControlEvent(close_event);

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

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
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0 =
      InitRecvEvent(absl::StrCat(kResp0, kResp1.substr(0, kResp1.length() / 2)));
  std::unique_ptr<SocketDataEvent> event1 = InitRecvEvent(kResp1.substr(kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event2 = InitRecvEvent(kResp2);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(event0));
  source_->AcceptDataEvent(std::move(event2));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(2, record_batch[0]->Size());

  source_->AcceptDataEvent(std::move(event1));
  source_->AcceptControlEvent(close_event);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(2, record_batch[0]->Size()) << "Late events won't get processed.";
}

TEST_F(SocketTraceConnectorTest, NoEvents) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0 = InitRecvEvent(kResp0);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptControlEvent(conn);

  // Check empty transfer.
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, record_batch[0]->Size());

  // Check empty transfer following a successful transfer.
  source_->AcceptDataEvent(std::move(event0));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, record_batch[0]->Size());
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, record_batch[0]->Size());

  EXPECT_EQ(1, source_->NumActiveConnections());
  source_->AcceptControlEvent(close_event);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
}

TEST_F(SocketTraceConnectorTest, RequestResponseMatching) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptControlEvent(close_event);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(3, record_batch[0]->Size());

  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "doe"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqMethodIdx]), ElementsAre("GET", "GET", "GET"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqPathIdx]),
              ElementsAre("/index.html", "/data.html", "/logs.html"));
}

TEST_F(SocketTraceConnectorTest, MissingEventInStream) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  std::unique_ptr<SocketDataEvent> req_event3 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event3 = InitRecvEvent(kResp0);
  // No Close event (connection still active).

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  PL_UNUSED(resp_event1);  // Missing event.
  source_->AcceptDataEvent(std::move(resp_event2));

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());
  EXPECT_EQ(2, record_batch[0]->Size());

  source_->AcceptDataEvent(std::move(req_event3));
  source_->AcceptDataEvent(std::move(resp_event3));

  // Processing of resp_event3 will result in one more record.
  // TODO(oazizi): Update this when req-resp matching algorithm is updated.
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());
  EXPECT_EQ(3, record_batch[0]->Size());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInOrder) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);

  EXPECT_EQ(0, source_->NumActiveConnections());

  source_->AcceptControlEvent(conn);

  EXPECT_EQ(1, source_->NumActiveConnections());
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());

  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));

  EXPECT_EQ(1, source_->NumActiveConnections());
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());

  source_->AcceptControlEvent(close_event);
  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnectionTracker::kDeathCountdownIters - 1; ++i) {
    EXPECT_EQ(1, source_->NumActiveConnections());
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  }

  EXPECT_EQ(1, source_->NumActiveConnections());
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, source_->NumActiveConnections());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupOutOfOrder) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);

  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptDataEvent(std::move(resp_event0));

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());

  source_->AcceptControlEvent(close_event);
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(req_event2));

  // CloseConnEvent results in countdown = kDeathCountdownIters.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnectionTracker::kDeathCountdownIters - 1; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, source_->NumActiveConnections());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupMissingDataEvent) {
  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> req_event3 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  std::unique_ptr<SocketDataEvent> resp_event3 = InitRecvEvent(kResp2);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);

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
  for (int32_t i = 0; i < ConnectionTracker::kDeathCountdownIters - 1; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, source_->NumActiveConnections());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupOldGenerations) {
  struct socket_control_event_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> conn0_resp_event = InitRecvEvent(kResp0);
  struct socket_control_event_t conn0_close = InitClose();

  struct socket_control_event_t conn1 = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> conn1_req_event = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> conn1_resp_event = InitRecvEvent(kResp1);
  struct socket_control_event_t conn1_close = InitClose();

  struct socket_control_event_t conn2 = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> conn2_req_event = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> conn2_resp_event = InitRecvEvent(kResp2);
  struct socket_control_event_t conn2_close = InitClose();

  DataTable data_table(kHTTPTable);

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

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());

  // TransferData results in countdown = kDeathCountdownIters for old generations.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnectionTracker::kDeathCountdownIters - 1; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, source_->NumActiveConnections());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupNoProtocol) {
  struct socket_control_event_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  struct socket_control_event_t conn0_close = InitClose();

  conn0.open.traffic_class.protocol = kProtocolUnknown;

  DataTable data_table(kHTTPTable);

  source_->AcceptControlEvent(conn0);
  source_->AcceptControlEvent(conn0_close);

  // TransferData results in countdown = kDeathCountdownIters for old generations.

  // Death countdown period: keep calling Transfer Data to increment iterations.
  for (int32_t i = 0; i < ConnectionTracker::kDeathCountdownIters - 1; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, source_->NumActiveConnections());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInactiveDead) {
  ConnectionTracker::SetInactivityDuration(std::chrono::seconds(1));

  // Inactive dead connections are determined by checking the /proc filesystem.
  // Here we create a PID that is a valid number, but non-existent on any Linux system.
  // Note that max PID bits in Linux is 22 bits.
  uint32_t impossible_pid = 1 << 23;

  struct socket_control_event_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  conn0.open.conn_id.pid = impossible_pid;

  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent(kReq0);
  conn0_req_event->attr.conn_id.pid = impossible_pid;

  std::unique_ptr<SocketDataEvent> conn0_resp_event = InitRecvEvent(kResp0);
  conn0_resp_event->attr.conn_id.pid = impossible_pid;

  struct socket_control_event_t conn0_close = InitClose();
  conn0_close.close.conn_id.pid = impossible_pid;

  DataTable data_table(kHTTPTable);

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));
  source_->AcceptDataEvent(std::move(conn0_resp_event));
  PL_UNUSED(conn0_close);  // Missing close event.

  for (int i = 0; i < 100; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  sleep(2);

  // Connection should be timed out by now, and should be killed by one more TransferData() call.

  EXPECT_EQ(1, source_->NumActiveConnections());
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(0, source_->NumActiveConnections());
}

TEST_F(SocketTraceConnectorTest, ConnectionCleanupInactiveAlive) {
  ConnectionTracker::SetInactivityDuration(std::chrono::seconds(1));

  // Inactive alive connections are determined by checking the /proc filesystem.
  // Here we create a PID that is a real PID, by using the test process itself.
  // And we create a real FD, by using FD 1, which is stdout.

  uint32_t real_pid = getpid();
  uint32_t real_fd = 1;

  struct socket_control_event_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  conn0.open.conn_id.pid = real_pid;
  conn0.open.conn_id.fd = real_fd;

  // An incomplete message means it shouldn't be parseable (we don't want TranfserData to succeed).
  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent("GET /index.html HTTP/1.1\r\n");
  conn0_req_event->attr.conn_id.pid = real_pid;
  conn0_req_event->attr.conn_id.fd = real_fd;

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));

  for (int i = 0; i < 100; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  conn_id_t search_conn_id;
  search_conn_id.pid = real_pid;
  search_conn_id.fd = real_fd;
  search_conn_id.generation = 1;
  const ConnectionTracker* tracker = source_->GetConnectionTracker(search_conn_id);
  ASSERT_NE(nullptr, tracker);

  sleep(2);

  // Connection should be timed out by next TransferData,
  // which should also cause events to be flushed, but the connection is still alive.

  EXPECT_EQ(1, source_->NumActiveConnections());
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());

  // Should not have transferred any data.
  EXPECT_EQ(0, record_batch[0]->Size());

  // Events should have been flushed.
  EXPECT_TRUE(tracker->recv_data().Empty<http::HTTPMessage>());
  EXPECT_TRUE(tracker->send_data().Empty<http::HTTPMessage>());
}

TEST_F(SocketTraceConnectorTest, MySQLPrepareExecuteClose) {
  FLAGS_stirling_enable_mysql_tracing = true;

  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolMySQL);
  std::unique_ptr<SocketDataEvent> prepare_req_event = InitSendEvent(mysql_stmt_prepare_req);
  std::vector<std::unique_ptr<SocketDataEvent>> prepare_resp_events;
  for (std::string resp_packet : mysql_stmt_prepare_resp) {
    prepare_resp_events.push_back(InitRecvEvent(resp_packet));
  }

  std::unique_ptr<SocketDataEvent> execute_req_event = InitSendEvent(mysql_stmt_execute_req);
  std::vector<std::unique_ptr<SocketDataEvent>> execute_resp_events;
  for (std::string resp_packet : mysql_stmt_execute_resp) {
    execute_resp_events.push_back(InitRecvEvent(resp_packet));
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

  DataTable data_table(kMySQLTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(2, column->Size());
  }

  std::string expected_entry0 =
      "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
      "JOIN sock_tag ON "
      "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=? "
      "GROUP "
      "BY id ORDER BY ?";

  std::string expected_entry1 =
      "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
      "JOIN sock_tag ON "
      "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
      "GROUP "
      "BY id ORDER BY id";

  EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]),
              ElementsAre(expected_entry0, expected_entry1));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]),
              ElementsAre("", "Resultset rows = 2"));

  // Test execute fail after close. It should create an entry with the Error.
  std::unique_ptr<SocketDataEvent> close_req_event = InitSendEvent(mysql_stmt_close_req);
  std::unique_ptr<SocketDataEvent> execute_req_event2 = InitSendEvent(mysql_stmt_execute_req);
  std::unique_ptr<SocketDataEvent> execute_resp_event2 = InitRecvEvent(mysql_err_resp);

  source_->AcceptDataEvent(std::move(close_req_event));
  source_->AcceptDataEvent(std::move(execute_req_event2));
  source_->AcceptDataEvent(std::move(execute_resp_event2));
  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(4, column->Size());
  }

  EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]),
              ElementsAre(expected_entry0, expected_entry1, "", ""));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]),
              ElementsAre("", "Resultset rows = 2", "", "This is an error."));
  // In test environment, latencies are simply the number of packets in the response.
  // StmtPrepare resp has 7 response packets: 1 header + 2 col defs + 1 EOF + 2 param defs + 1 EOF.
  // StmtExecute resp has 7 response packets: 1 header + 2 col defs + 1 EOF + 2 rows + 1 EOF.
  // StmtClose resp has 0 response packets.
  // StmtExecute resp has 1 response packet: 1 error.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]),
              ElementsAre(7, 7, 0, 1));
}

TEST_F(SocketTraceConnectorTest, MySQLQuery) {
  FLAGS_stirling_enable_mysql_tracing = true;

  struct socket_control_event_t conn = InitConn(TrafficProtocol::kProtocolMySQL);
  std::unique_ptr<SocketDataEvent> query_req_event = InitSendEvent(mysql_query_req);
  std::vector<std::unique_ptr<SocketDataEvent>> query_resp_events;
  for (std::string resp_packet : mysql_query_resp) {
    query_resp_events.push_back(InitRecvEvent(resp_packet));
  }

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(query_req_event));
  for (auto& query_resp_event : query_resp_events) {
    source_->AcceptDataEvent(std::move(query_resp_event));
  }

  DataTable data_table(kMySQLTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size());
  }

  EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]), ElementsAre("SELECT name FROM tag;"));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]), ElementsAre("Resultset rows = 3"));
  // In test environment, latencies are simply the number of packets in the response.
  // In this case 7 response packets: 1 header + 1 col defs + 1 EOF + 3 rows + 1 EOF.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(7));
}

}  // namespace stirling
}  // namespace pl
