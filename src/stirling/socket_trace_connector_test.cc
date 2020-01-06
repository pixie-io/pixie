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
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/events_fixture.h"
#include "src/stirling/testing/http2_stream_generator.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::ColWrapperSizeIs;
using ::testing::Each;
using ::testing::ElementsAre;

using RecordBatch = types::ColumnWrapperRecordBatch;

class SocketTraceConnectorTest : public testing::EventsFixture {
 protected:
  static constexpr uint32_t kASID = 1;

  void SetUp() override {
    // Create and configure the connector.
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
  static constexpr int kMySQLReqCmdIdx = kMySQLTable.ColIndex("req_cmd");
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> event0_json = InitRecvEvent<kProtocolHTTP>(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_text = InitRecvEvent<kProtocolHTTP>(kTextResp);
  std::unique_ptr<SocketDataEvent> event2_text = InitRecvEvent<kProtocolHTTP>(kTextResp);
  std::unique_ptr<SocketDataEvent> event3_json = InitRecvEvent<kProtocolHTTP>(kJSONResp);
  struct socket_control_event_t close_event = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  EXPECT_NE(0, source_->ClockRealTimeOffset());

  // Registers a new connection.
  source_->AcceptControlEvent(conn);

  ASSERT_THAT(source_->NumActiveConnections(), 1);

  conn_id_t search_conn_id;
  search_conn_id.upid.pid = kPID;
  search_conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> event0_json = InitRecvEvent<kProtocolHTTP>(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_json = InitRecvEvent<kProtocolHTTP>(kJSONResp);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> event0 =
      InitRecvEvent<kProtocolHTTP>(absl::StrCat(kResp0, kResp1.substr(0, kResp1.length() / 2)));
  std::unique_ptr<SocketDataEvent> event1 =
      InitRecvEvent<kProtocolHTTP>(kResp1.substr(kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event2 = InitRecvEvent<kProtocolHTTP>(kResp2);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> event0 = InitRecvEvent<kProtocolHTTP>(kResp0);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent<kProtocolHTTP>(kResp2);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent<kProtocolHTTP>(kResp2);
  std::unique_ptr<SocketDataEvent> req_event3 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event3 = InitRecvEvent<kProtocolHTTP>(kResp0);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent<kProtocolHTTP>(kResp2);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent<kProtocolHTTP>(kResp2);
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
  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> req_event3 = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent<kProtocolHTTP>(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent<kProtocolHTTP>(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent<kProtocolHTTP>(kResp2);
  std::unique_ptr<SocketDataEvent> resp_event3 = InitRecvEvent<kProtocolHTTP>(kResp2);
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
  struct socket_control_event_t conn0 = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent<kProtocolHTTP>(kReq0);
  std::unique_ptr<SocketDataEvent> conn0_resp_event = InitRecvEvent<kProtocolHTTP>(kResp0);
  struct socket_control_event_t conn0_close = InitClose();

  struct socket_control_event_t conn1 = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> conn1_req_event = InitSendEvent<kProtocolHTTP>(kReq1);
  std::unique_ptr<SocketDataEvent> conn1_resp_event = InitRecvEvent<kProtocolHTTP>(kResp1);
  struct socket_control_event_t conn1_close = InitClose();

  struct socket_control_event_t conn2 = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> conn2_req_event = InitSendEvent<kProtocolHTTP>(kReq2);
  std::unique_ptr<SocketDataEvent> conn2_resp_event = InitRecvEvent<kProtocolHTTP>(kResp2);
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
  struct socket_control_event_t conn0 = InitConn<kProtocolHTTP>();
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
  // Inactive dead connections are determined by checking the /proc filesystem.
  // Here we create a PID that is a valid number, but non-existent on any Linux system.
  // Note that max PID bits in Linux is 22 bits.
  const uint32_t impossible_pid = 1 << 23;

  struct socket_control_event_t conn0 = InitConn<kProtocolHTTP>();
  conn0.open.conn_id.upid.pid = impossible_pid;

  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent<kProtocolHTTP>(kReq0);
  conn0_req_event->attr.conn_id.upid.pid = impossible_pid;

  std::unique_ptr<SocketDataEvent> conn0_resp_event = InitRecvEvent<kProtocolHTTP>(kResp0);
  conn0_resp_event->attr.conn_id.upid.pid = impossible_pid;

  DataTable data_table(kHTTPTable);

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptControlEvent(conn0);
  source_->AcceptDataEvent(std::move(conn0_req_event));
  source_->AcceptDataEvent(std::move(conn0_resp_event));

  // Note that close event was not recorded, so this connection remain open before reaching the
  // inactivity threshold.

  // First set the inactive duration threshold to be artificially large, so that the next loop
  // checking the number of active connections is robust.
  ConnectionTracker::SetInactivityDuration(std::chrono::seconds(1000));
  for (int i = 0; i < 100; ++i) {
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    EXPECT_EQ(1, source_->NumActiveConnections());
  }

  // Then reduce the threshold to 0, so that any connections would be considered dead.
  ConnectionTracker::SetInactivityDuration(std::chrono::seconds(0));
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

  struct socket_control_event_t conn0 = InitConn<kProtocolHTTP>();
  conn0.open.conn_id.upid.pid = real_pid;
  conn0.open.conn_id.fd = real_fd;

  // An incomplete message means it shouldn't be parseable (we don't want TranfserData to succeed).
  std::unique_ptr<SocketDataEvent> conn0_req_event =
      InitSendEvent<kProtocolHTTP>("GET /index.html HTTP/1.1\r\n");
  conn0_req_event->attr.conn_id.upid.pid = real_pid;
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
  search_conn_id.upid.pid = real_pid;
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

//-----------------------------------------------------------------------------
// MySQL specific tests
//-----------------------------------------------------------------------------

TEST_F(SocketTraceConnectorTest, MySQLPrepareExecuteClose) {
  struct socket_control_event_t conn = InitConn<kProtocolMySQL>();
  std::unique_ptr<SocketDataEvent> prepare_req_event =
      InitSendEvent<kProtocolMySQL>(mysql_stmt_prepare_req);
  std::vector<std::unique_ptr<SocketDataEvent>> prepare_resp_events;
  for (std::string resp_packet : mysql_stmt_prepare_resp) {
    prepare_resp_events.push_back(InitRecvEvent<kProtocolMySQL>(resp_packet));
  }

  std::unique_ptr<SocketDataEvent> execute_req_event =
      InitSendEvent<kProtocolMySQL>(mysql_stmt_execute_req);
  std::vector<std::unique_ptr<SocketDataEvent>> execute_resp_events;
  for (std::string resp_packet : mysql_stmt_execute_resp) {
    execute_resp_events.push_back(InitRecvEvent<kProtocolMySQL>(resp_packet));
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
  std::unique_ptr<SocketDataEvent> close_req_event =
      InitSendEvent<kProtocolMySQL>(mysql_stmt_close_req);
  std::unique_ptr<SocketDataEvent> execute_req_event2 =
      InitSendEvent<kProtocolMySQL>(mysql_stmt_execute_req);
  std::unique_ptr<SocketDataEvent> execute_resp_event2 =
      InitRecvEvent<kProtocolMySQL>(mysql_err_resp);

  source_->AcceptDataEvent(std::move(close_req_event));
  source_->AcceptDataEvent(std::move(execute_req_event2));
  source_->AcceptDataEvent(std::move(execute_resp_event2));
  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);

  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(4)));
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
  DataTable data_table(kMySQLTable);

  struct socket_control_event_t conn = InitConn<kProtocolMySQL>();
  std::unique_ptr<SocketDataEvent> query_req_event = InitSendEvent<kProtocolMySQL>(mysql_query_req);
  std::vector<std::unique_ptr<SocketDataEvent>> query_resp_events;
  for (std::string resp_packet : mysql_query_resp) {
    query_resp_events.push_back(InitRecvEvent<kProtocolMySQL>(resp_packet));
  }

  source_->AcceptControlEvent(conn);
  source_->AcceptDataEvent(std::move(query_req_event));
  for (auto& query_resp_event : query_resp_events) {
    source_->AcceptDataEvent(std::move(query_resp_event));
  }

  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(1)));

  EXPECT_THAT(ToStringVector(record_batch[kMySQLReqBodyIdx]), ElementsAre("SELECT name FROM tag;"));
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]), ElementsAre("Resultset rows = 3"));
  // In test environment, latencies are simply the number of packets in the response.
  // In this case 7 response packets: 1 header + 1 col defs + 1 EOF + 3 rows + 1 EOF.
  EXPECT_THAT(ToIntVector<types::Int64Value>(record_batch[kMySQLLatencyIdx]), ElementsAre(7));
}

TEST_F(SocketTraceConnectorTest, MySQLMultipleCommands) {
  DataTable data_table(kMySQLTable);

  struct socket_control_event_t conn = InitConn<kProtocolMySQL>();

  // The following is a captured trace while running a script on a real instance of MySQL.
  std::vector<std::unique_ptr<SocketDataEvent>> events;
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x21\x00\x00\x00"
                                                     "\x03"
                                                     "select @@version_comment limit 1")}));
  events.push_back(InitRecvEvent<kProtocolMySQL>({ConstStringView(
      "\x01\x00\x00\x01"
      "\x01\x27\x00\x00\x02\x03"
      "def"
      "\x00\x00\x00\x11"
      "@@version_comment"
      "\x00\x0C\x21\x00\x18\x00\x00\x00\xFD\x00\x00\x1F\x00\x00\x09\x00\x00\x03\x08"
      "(Ubuntu)"
      "\x07\x00\x00\x04\xFE\x00\x00\x02\x00\x00\x00")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x22\x00\x00\x00"
                                                     "\x03"
                                                     "DROP DATABASE IF EXISTS employees")}));
  events.push_back(
      InitRecvEvent<kProtocolMySQL>({ConstStringView("\x07\x00\x00\x01"
                                                     "\x00\x00\x00\x02\x01\x00\x00")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x28\x00\x00\x00"
                                                     "\x03"
                                                     "CREATE DATABASE IF NOT EXISTS employees")}));
  events.push_back(
      InitRecvEvent<kProtocolMySQL>({ConstStringView("\x07\x00\x00\x01"
                                                     "\x00\x01\x00\x02\x00\x00\x00")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x12\x00\x00\x00"
                                                     "\x03"
                                                     "SELECT DATABASE()")}));
  events.push_back(InitRecvEvent<kProtocolMySQL>(
      {ConstStringView("\x01\x00\x00\x01"
                       "\x01\x20\x00\x00\x02\x03"
                       "def"
                       "\x00\x00\x00\x0A"
                       "DATABASE()"
                       "\x00\x0C\x21\x00\x66\x00\x00\x00\xFD\x00\x00\x1F\x00\x00\x01\x00\x00\x03"
                       "\xFB\x07\x00\x00\x04\xFE\x00\x00\x02\x00\x00\x00")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x0A\x00\x00\x00"
                                                     "\x02"
                                                     "employees")}));
  events.push_back(InitRecvEvent<kProtocolMySQL>(
      {ConstStringView("\x15\x00\x00\x01"
                       "\x00\x00\x00\x02\x40\x00\x00\x00\x0C\x01\x0A\x09"
                       "employees")}));
  events.push_back(InitSendEvent<kProtocolMySQL>(
      {ConstStringView("\x2f\x00\x00\x00"
                       "\x03"
                       "SELECT 'CREATING DATABASE STRUCTURE' as 'INFO'")}));
  events.push_back(InitRecvEvent<kProtocolMySQL>({ConstStringView(
      "\x01\x00\x00\x01"
      "\x01\x1A\x00\x00\x02\x03"
      "def"
      "\x00\x00\x00\x04"
      "INFO"
      "\x00\x0C\x21\x00\x51\x00\x00\x00\xFD\x01\x00\x1F\x00\x00\x1C\x00\x00\x03\x1B"
      "CREATING DATABASE STRUCTURE"
      "\x07\x00\x00\x04\xFE\x00\x00\x02\x00\x00\x00")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\xC1\x00\x00\x00"
                                                     "\x03"
                                                     "DROP TABLE IF EXISTS dept_emp,\n"
                                                     "                     dept_manager,\n"
                                                     "                     titles,\n"
                                                     "                     salaries, \n"
                                                     "                     employees, \n"
                                                     "                     departments")}));
  events.push_back(
      InitRecvEvent<kProtocolMySQL>({ConstStringView("\x07\x00\x00\x01"
                                                     "\x00\x00\x00\x02\x00\x06\x00")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x1C\x00\x00\x00"
                                                     "\x03"
                                                     "set storage_engine = InnoDB")}));
  events.push_back(
      InitRecvEvent<kProtocolMySQL>({ConstStringView("\x31\x00\x00\x01"
                                                     "\xFF\xA9\x04\x23"
                                                     "HY000"
                                                     "Unknown system variable 'storage_engine'")}));
  events.push_back(
      InitSendEvent<kProtocolMySQL>({ConstStringView("\x01\x00\x00\x00"
                                                     "\x01")}));

  source_->AcceptControlEvent(conn);
  for (auto& event : events) {
    source_->AcceptDataEvent(std::move(event));
  }

  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  EXPECT_THAT(record_batch, Each(ColWrapperSizeIs(8)));

  // In this test environment, latencies are the number of events.

  int idx = 0;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "select @@version_comment limit 1");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "DROP DATABASE IF EXISTS employees");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "CREATE DATABASE IF NOT EXISTS employees");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "SELECT DATABASE()");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "employees");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kInitDB));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "SELECT 'CREATING DATABASE STRUCTURE' as 'INFO'");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "DROP TABLE IF EXISTS dept_emp,\n                     dept_manager,\n                  "
            "   titles,\n                     salaries, \n                     employees, \n       "
            "              departments");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx), 1);

  ++idx;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "set storage_engine = InnoDB");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx),
            "Unknown system variable 'storage_engine'");
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx).val, 1);
}

// Inspired from real traced query.
// Number of resultset rows is large enough to cause a sequence ID rollover.
TEST_F(SocketTraceConnectorTest, MySQLQueryWithLargeResultset) {
  DataTable data_table(kMySQLTable);

  struct socket_control_event_t conn = InitConn<kProtocolMySQL>();

  // The following is a captured trace while running a script on a real instance of MySQL.
  std::vector<std::unique_ptr<SocketDataEvent>> events;
  events.push_back(InitSendEvent<kProtocolMySQL>(mysql::testutils::GenRequestPacket(
      mysql::MySQLEventType::kQuery, "SELECT emp_no FROM employees WHERE emp_no < 15000;")));

  // Sequence ID of zero is the request.
  int seq_id = 1;

  // First packet: number of columns in the query.
  events.push_back(InitRecvEvent<kProtocolMySQL>(
      mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(1))));
  // The column def packet (a bunch of length-encoded strings).
  events.push_back(InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
      seq_id++, mysql::testutils::LengthEncodedString("def") +
                    mysql::testutils::LengthEncodedString("employees") +
                    mysql::testutils::LengthEncodedString("employees") +
                    mysql::testutils::LengthEncodedString("employees") +
                    mysql::testutils::LengthEncodedString("emp_no") +
                    mysql::testutils::LengthEncodedString("emp_no") +
                    mysql::testutils::LengthEncodedString(
                        ConstStringView("\x3F\x00\x0B\x00\x00\x00\x03\x03\x50\x00\x00\x00")))));
  // A bunch of resultset rows.
  for (int id = 10001; id < 19999; ++id) {
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(id))));
  }
  // Final OK/EOF packet.
  events.push_back(InitRecvEvent<kProtocolMySQL>(
      mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\xFE\x00\x00\x02\x00\x00\x00"))));

  source_->AcceptControlEvent(conn);
  for (auto& event : events) {
    source_->AcceptDataEvent(std::move(event));
  }

  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  int idx = 0;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx),
            "SELECT emp_no FROM employees WHERE emp_no < 15000;");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx), "Resultset rows = 9998");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
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
  DataTable data_table(kMySQLTable);

  struct socket_control_event_t conn = InitConn<kProtocolMySQL>();

  // The following is a captured trace while running a script on a real instance of MySQL.
  std::vector<std::unique_ptr<SocketDataEvent>> events;
  events.push_back(InitSendEvent<kProtocolMySQL>(
      mysql::testutils::GenRequestPacket(mysql::MySQLEventType::kQuery, "CALL multi()")));

  // Sequence ID of zero is the request.
  int seq_id = 1;

  // First resultset.
  {
    // First packet: number of columns in the query.
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(1))));
    // The column def packet (a bunch of length-encoded strings).
    events.push_back(InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
        seq_id++,
        mysql::testutils::LengthEncodedString("def") +
            ConstString(
                "\x00\x00\x00\x01\x31\x00\x0C\x3F\x00\x01\x00\x00\x00\x08\x81\x00\x00\x00\x00"))));
    // A resultset row.
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedString("1"))));
    // OK/EOF packet with SERVER_MORE_RESULTS_EXISTS flag set.
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\xFE\x00\x00\x0A\x00\x00\x00"))));
  }

  // Second resultset.
  {
    // First packet: number of columns in the query.
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedInt(1))));
    // The column def packet (a bunch of length-encoded strings).
    events.push_back(InitRecvEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(
        seq_id++,
        mysql::testutils::LengthEncodedString("def") +
            ConstString(
                "\x00\x00\x00\x01\x31\x00\x0C\x3F\x00\x01\x00\x00\x00\x08\x81\x00\x00\x00\x00"))));
    // A resultset row.
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, mysql::testutils::LengthEncodedString("1"))));
    // OK/EOF packet with SERVER_MORE_RESULTS_EXISTS flag set.
    events.push_back(InitRecvEvent<kProtocolMySQL>(
        mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\xFE\x00\x00\x0A\x00\x00\x00"))));
  }

  // Final OK packet, signaling end of multi-resultset.
  events.push_back(InitRecvEvent<kProtocolMySQL>(
      mysql::testutils::GenRawPacket(seq_id++, ConstStringView("\x00\x01\x00\x02\x00\x00\x00"))));

  source_->AcceptControlEvent(conn);
  for (auto& event : events) {
    source_->AcceptDataEvent(std::move(event));
  }

  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  int idx = 0;
  EXPECT_EQ(record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx), "CALL multi()");
  EXPECT_EQ(record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx),
            "Resultset rows = 1, Resultset rows = 1");
  EXPECT_EQ(record_batch[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx),
            static_cast<int>(mysql::MySQLEventType::kQuery));
  EXPECT_EQ(record_batch[kMySQLLatencyIdx]->Get<types::Int64Value>(idx).val, 9);
}

//-----------------------------------------------------------------------------
// HTTP2 specific tests
//-----------------------------------------------------------------------------

TEST_F(SocketTraceConnectorTest, HTTP2ClientTest) {
  DataTable data_table(kHTTPTable);

  auto conn = InitConn<kProtocolHTTP2Uprobe>();

  auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, 7);

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
  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 5);
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
  DataTable data_table(kHTTPTable);

  auto conn = InitConn<kProtocolHTTP2Uprobe>();

  auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, 8);

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
  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 5);
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
  DataTable data_table(kHTTPTable);

  auto conn = InitConn<kProtocolHTTP2Uprobe>();

  auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, 7);

  source_->AcceptControlEvent(conn);
  // Request headers are missing to model mid-stream capture.
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "uest");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 1);
}

// This test models capturing data mid-stream, where we may have missed the request entirely.
TEST_F(SocketTraceConnectorTest, HTTP2ResponseOnly) {
  DataTable data_table(kHTTPTable);

  auto conn = InitConn<kProtocolHTTP2Uprobe>();

  auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, 7);

  source_->AcceptControlEvent(conn);
  // Request missing to model mid-stream capture.
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(0)));

  // TODO(oazizi): Someday we will need to capture response only streams properly.
  // In that case, we would expect certain values here.
  // EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "onse");
  // EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), TBD);
}

// This test models capturing data mid-stream, where we may have missed the request entirely.
TEST_F(SocketTraceConnectorTest, HTTP2SpanAcrossTransferData) {
  DataTable data_table(kHTTPTable);

  auto conn = InitConn<kProtocolHTTP2Uprobe>();

  auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, 7);

  source_->AcceptControlEvent(conn);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  // TransferData should not have pushed data to the tables, because HTTP2 stream is still active.
  ASSERT_THAT(*data_table.ActiveRecordBatch(), Each(ColWrapperSizeIs(0)));

  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());
  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  // TransferData should now have pushed data to the tables, because HTTP2 stream has ended.
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 5);
}

// This test models multiple streams back-to-back.
TEST_F(SocketTraceConnectorTest, HTTP2SequentialStreams) {
  DataTable data_table(kHTTPTable);

  std::vector<int> stream_ids = {7, 9, 11, 13};

  auto conn = InitConn<kProtocolHTTP2Uprobe>();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, stream_id);
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

  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response7");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request13");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response13");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 6);
}

// This test models multiple streams running in parallel.
TEST_F(SocketTraceConnectorTest, HTTP2ParallelStreams) {
  DataTable data_table(kHTTPTable);

  std::vector<uint32_t> stream_ids = {7, 9, 11, 13};
  std::map<uint32_t, testing::StreamEventGenerator> frame_generators;

  auto conn = InitConn<kProtocolHTTP2Uprobe>();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    frame_generators.insert(
        {stream_id, testing::StreamEventGenerator(conn.open.conn_id, stream_id)});
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
  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response7");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request13");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response13");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 6);
}

// This test models one stream start and ending within the span of a larger stream.
// Random TransferData calls are interspersed just to make things more fun :)
TEST_F(SocketTraceConnectorTest, HTTP2StreamSandwich) {
  DataTable data_table(kHTTPTable);

  auto conn = InitConn<kProtocolHTTP2Uprobe>();
  source_->AcceptControlEvent(conn);

  uint32_t stream_id = 7;

  auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, stream_id);
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "post"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic"));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("Req"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>("uest"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventWrite>(
      std::to_string(stream_id), /* end_stream */ true));

  {
    uint32_t stream_id2 = 9;
    auto frame_generator2 = testing::StreamEventGenerator(conn.open.conn_id, stream_id2);
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":method", "post"));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":host", "pixie.ai"));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventWrite>(":path", "/magic"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>("Req"));
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>("uest"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventWrite>(
        std::to_string(stream_id2), /* end_stream */ true));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventRead>("Resp"));
    source_->AcceptHTTP2Data(frame_generator2.GenDataFrame<kDataFrameEventRead>("onse"));
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    source_->AcceptHTTP2Data(
        frame_generator2.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id2)));
    source_->AcceptHTTP2Header(frame_generator2.GenHeader<kHeaderEventRead>(":status", "200"));
    source_->AcceptHTTP2Header(frame_generator2.GenEndStreamHeader<kHeaderEventRead>());
  }

  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("Resp"));
  source_->AcceptHTTP2Data(frame_generator.GenDataFrame<kDataFrameEventRead>("onse"));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  source_->AcceptHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventRead>(std::to_string(stream_id)));
  source_->AcceptHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));
  source_->AcceptHTTP2Header(frame_generator.GenEndStreamHeader<kHeaderEventRead>());

  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  // Note that the records are pushed as soon as they complete. This is so
  // a long-running stream does not block other shorter streams from being recorded.
  // Notice, however, that this causes stream_id 9 to appear before stream_id 7.

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(2)));
  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request9");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response9");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response7");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 6);
}

// This test models an old stream appearing slightly late.
TEST_F(SocketTraceConnectorTest, HTTP2StreamIDRace) {
  DataTable data_table(kHTTPTable);

  std::vector<int> stream_ids = {7, 9, 5, 11};

  auto conn = InitConn<kProtocolHTTP2Uprobe>();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, stream_id);
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

  source_->AcceptControlEvent(InitClose());

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));

  // Note that the order in which the events are emitted are actually ordered by stream ID,
  // even though the events of stream ID 5 came late.
  // This would not necessary have been the case if the late-arriving stream had been after
  // a call to TransferData().

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request5");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response5");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request7");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response7");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(2), "Request9");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(2), "Response9");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(2), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request11");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response11");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 6);
}

// This test models an old stream appearing out-of-nowhere.
// Expectation is that we should be robust in such cases.
TEST_F(SocketTraceConnectorTest, HTTP2OldStream) {
  DataTable data_table(kHTTPTable);

  std::vector<int> stream_ids = {117, 119, 3, 121};

  auto conn = InitConn<kProtocolHTTP2Uprobe>();
  source_->AcceptControlEvent(conn);

  for (auto stream_id : stream_ids) {
    auto frame_generator = testing::StreamEventGenerator(conn.open.conn_id, stream_id);
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

    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  }

  source_->AcceptControlEvent(InitClose());

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(4)));

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(0), "Request117");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0), "Response117");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(0), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(1), "Request119");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1), "Response119");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(1), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(2), "Request3");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(2), "Response3");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(2), 6);

  EXPECT_EQ(record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(3), "Request121");
  EXPECT_EQ(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(3), "Response121");
  EXPECT_EQ(record_batch[kHTTPLatencyIdx]->Get<types::Int64Value>(3), 6);
}

}  // namespace stirling
}  // namespace pl
