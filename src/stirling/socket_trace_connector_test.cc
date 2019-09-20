#include "src/stirling/socket_trace_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/socket.h>
#include <memory>

#include "src/shared/metadata/metadata.h"
#include "src/stirling/bcc_bpf/socket_trace.h"

#include "src/stirling/data_table.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"

namespace pl {
namespace stirling {

using ::testing::ElementsAre;
using RecordBatch = types::ColumnWrapperRecordBatch;

class SocketTraceConnectorTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kASID = 1;
  static constexpr uint32_t kPID = 12345;
  static constexpr uint64_t kPIDStartTimeNS = 112358;
  static constexpr uint32_t kFD = 3;

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

  conn_info_t InitConn(TrafficProtocol protocol) {
    ++generation_;
    ++current_timestamp_;
    send_seq_num_ = 0;
    recv_seq_num_ = 0;

    conn_info_t conn_info{};
    conn_info.addr.sin6_family = AF_INET;
    conn_info.timestamp_ns = current_timestamp_;
    conn_info.conn_id.pid = kPID;
    conn_info.conn_id.pid_start_time_ns = kPIDStartTimeNS;
    conn_info.conn_id.fd = kFD;
    conn_info.conn_id.generation = generation_;
    conn_info.traffic_class.protocol = protocol;
    conn_info.traffic_class.role = kRoleRequestor;
    conn_info.rd_seq_num = 0;
    conn_info.wr_seq_num = 0;
    return conn_info;
  }

  std::unique_ptr<SocketDataEvent> InitSendEvent(std::string_view msg) {
    std::unique_ptr<SocketDataEvent> event = InitDataEvent(TrafficDirection::kEgress, msg);
    event->attr.seq_num = send_seq_num_;
    send_seq_num_++;
    return event;
  }

  std::unique_ptr<SocketDataEvent> InitRecvEvent(std::string_view msg) {
    std::unique_ptr<SocketDataEvent> event = InitDataEvent(TrafficDirection::kIngress, msg);
    event->attr.seq_num = recv_seq_num_;
    recv_seq_num_++;
    return event;
  }

  std::unique_ptr<SocketDataEvent> InitDataEvent(TrafficDirection direction, std::string_view msg) {
    ++current_timestamp_;

    socket_data_event_t event = {};
    event.attr.direction = direction;
    event.attr.traffic_class.protocol = kProtocolHTTP;
    event.attr.traffic_class.role = kRoleRequestor;
    event.attr.timestamp_ns = current_timestamp_;
    event.attr.conn_id.pid = kPID;
    event.attr.conn_id.pid_start_time_ns = kPIDStartTimeNS;
    event.attr.conn_id.fd = kFD;
    event.attr.conn_id.generation = generation_;
    event.attr.msg_size = msg.size();
    msg.copy(event.msg, msg.size());
    return std::make_unique<SocketDataEvent>(&event);
  }

  conn_info_t InitClose() {
    ++current_timestamp_;

    conn_info_t conn_info{};
    conn_info.timestamp_ns = current_timestamp_;
    conn_info.conn_id.pid = kPID;
    conn_info.conn_id.pid_start_time_ns = kPIDStartTimeNS;
    conn_info.conn_id.fd = kFD;
    conn_info.conn_id.generation = generation_;
    conn_info.rd_seq_num = recv_seq_num_;
    conn_info.wr_seq_num = send_seq_num_;
    return conn_info;
  }

  uint32_t generation_ = 0;
  uint64_t current_timestamp_ = 0;
  uint64_t send_seq_num_ = 0;
  uint64_t recv_seq_num_ = 0;

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
  static constexpr DataTableSchema kMySQLTable = SocketTraceConnector::kMySQLTable;
  static constexpr int kMySQLRespBodyIdx = kMySQLTable.ColIndex("body");

  std::string mySQLStmtPrepareReq;
  std::vector<std::string> mySQLStmtPrepareResp;
  std::string mySQLStmtExecuteReq;
  std::vector<std::string> mySQLStmtExecuteResp;
  std::string mySQLStmtCloseReq;
  std::string mySQLErrResp;

  std::string mySQLQueryReq;
  std::vector<std::string> mySQLQueryResp;

  void InitMySQLData() {
    mySQLStmtPrepareReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStringRequest(mysql::testutils::kStmtPrepareRequest,
                                              mysql::MySQLEventType::kComStmtPrepare)
               .msg);

    std::deque<mysql::Packet> prepare_packets =
        mysql::testutils::GenStmtPrepareOKResponse(mysql::testutils::kStmtPrepareResponse);
    for (int i = 0; i < static_cast<int>(prepare_packets.size()); ++i) {
      // i + 1 because packet_num 0 is the request, so response starts from 1.
      mySQLStmtPrepareResp.push_back(mysql::testutils::GenRawPacket(i + 1, prepare_packets[i].msg));
    }

    mySQLStmtExecuteReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStmtExecuteRequest(mysql::testutils::kStmtExecuteRequest).msg);

    std::deque<mysql::Packet> execute_packets =
        mysql::testutils::GenResultset(mysql::testutils::kStmtExecuteResultset);
    for (int i = 0; i < static_cast<int>(execute_packets.size()); ++i) {
      // i + 1 because packet_num 0 is the request, so response starts from 1.
      mySQLStmtExecuteResp.push_back(mysql::testutils::GenRawPacket(i + 1, execute_packets[i].msg));
    }

    mySQLErrResp = mysql::testutils::GenRawPacket(
        1, mysql::testutils::GenErr(mysql::ErrResponse(1096, "This an error.")).msg);

    mySQLStmtCloseReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStmtCloseRequest(mysql::testutils::kStmtCloseRequest).msg);

    mySQLQueryReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStringRequest(mysql::testutils::kQueryRequest,
                                              mysql::MySQLEventType::kComQuery)
               .msg);
    std::deque<mysql::Packet> query_packets =
        mysql::testutils::GenResultset(mysql::testutils::kQueryResultset);
    for (int i = 0; i < static_cast<int>(query_packets.size()); ++i) {
      // i + 1 because packet_num 0 is the request, so response starts from 1.
      mySQLQueryResp.push_back(mysql::testutils::GenRawPacket(i + 1, query_packets[i].msg));
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

template <class TValueType>
auto ToIntVector(const types::SharedColumnWrapper& col) {
  std::vector<int64_t> result;
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<TValueType>(i).val);
  }
  return result;
}

TEST_F(SocketTraceConnectorTest, End2End) {
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0_json = InitRecvEvent(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_text = InitRecvEvent(kTextResp);
  std::unique_ptr<SocketDataEvent> event2_text = InitRecvEvent(kTextResp);
  std::unique_ptr<SocketDataEvent> event3_json = InitRecvEvent(kJSONResp);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  EXPECT_NE(0, source_->ClockRealTimeOffset());

  // Registers a new connection.
  source_->AcceptOpenConnEvent(conn);

  ASSERT_THAT(source_->NumActiveConnections(), 1);

  conn_id_t search_conn_id;
  search_conn_id.pid = kPID;
  search_conn_id.pid_start_time_ns = kPIDStartTimeNS;
  search_conn_id.fd = kFD;
  search_conn_id.generation = 1;
  const ConnectionTracker* tracker = source_->GetConnectionTracker(search_conn_id);
  ASSERT_NE(nullptr, tracker);
  EXPECT_EQ(1 + source_->ClockRealTimeOffset(), tracker->conn().timestamp_ns);

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
  source_->AcceptCloseConnEvent(close_conn);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(3, column->Size())
        << "The filter is changed to require 'application/json' in Content-Type header, "
           "and event_json Content-Type matches, and is selected";
  }
  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "foo"));
  EXPECT_THAT(ToIntVector<types::Time64NSValue>(record_batch[kHTTPTimeIdx]),
              ElementsAre(2 + source_->ClockRealTimeOffset(), 4 + source_->ClockRealTimeOffset(),
                          5 + source_->ClockRealTimeOffset()));
}

TEST_F(SocketTraceConnectorTest, UPIDCheck) {
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0_json = InitRecvEvent(kJSONResp);
  std::unique_ptr<SocketDataEvent> event1_json = InitRecvEvent(kJSONResp);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  // Registers a new connection.
  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(std::move(event0_json)));
  source_->AcceptDataEvent(std::move(std::move(event1_json)));
  source_->AcceptCloseConnEvent(close_conn);

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  for (const auto& column : record_batch) {
    ASSERT_EQ(2, column->Size());
  }

  for (int i = 0; i < 2; ++i) {
    auto val = record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(i);
    md::UPID upid(val.val);

    EXPECT_EQ(upid.pid(), kPID);
    EXPECT_EQ(upid.start_ts(), kPIDStartTimeNS);
    EXPECT_EQ(upid.asid(), kASID);
  }
}

TEST_F(SocketTraceConnectorTest, AppendNonContiguousEvents) {
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0 =
      InitRecvEvent(absl::StrCat(kResp0, kResp1.substr(0, kResp1.length() / 2)));
  std::unique_ptr<SocketDataEvent> event1 = InitRecvEvent(kResp1.substr(kResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> event2 = InitRecvEvent(kResp2);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(event0));
  source_->AcceptDataEvent(std::move(event2));
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(2, record_batch[0]->Size());

  source_->AcceptDataEvent(std::move(event1));
  source_->AcceptCloseConnEvent(close_conn);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(2, record_batch[0]->Size()) << "Late events won't get processed.";
}

TEST_F(SocketTraceConnectorTest, NoEvents) {
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> event0 = InitRecvEvent(kResp0);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptOpenConnEvent(conn);

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
  source_->AcceptCloseConnEvent(close_conn);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
}

TEST_F(SocketTraceConnectorTest, RequestResponseMatching) {
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  source_->AcceptDataEvent(std::move(resp_event1));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptCloseConnEvent(close_conn);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(3, record_batch[0]->Size());

  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "doe"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqMethodIdx]), ElementsAre("GET", "GET", "GET"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqPathIdx]),
              ElementsAre("/index.html", "/data.html", "/logs.html"));
}

TEST_F(SocketTraceConnectorTest, MissingEventInStream) {
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
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

  source_->AcceptOpenConnEvent(conn);
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
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);

  EXPECT_EQ(0, source_->NumActiveConnections());

  source_->AcceptOpenConnEvent(conn);

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

  source_->AcceptCloseConnEvent(close_conn);
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
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);

  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptDataEvent(std::move(resp_event0));

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  EXPECT_EQ(1, source_->NumActiveConnections());

  source_->AcceptCloseConnEvent(close_conn);
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
  conn_info_t conn = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> req_event0 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> req_event1 = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> req_event2 = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> req_event3 = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> resp_event0 = InitRecvEvent(kResp0);
  std::unique_ptr<SocketDataEvent> resp_event1 = InitRecvEvent(kResp1);
  std::unique_ptr<SocketDataEvent> resp_event2 = InitRecvEvent(kResp2);
  std::unique_ptr<SocketDataEvent> resp_event3 = InitRecvEvent(kResp2);
  conn_info_t close_conn = InitClose();

  DataTable data_table(kHTTPTable);

  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(req_event0));
  source_->AcceptDataEvent(std::move(req_event1));
  source_->AcceptDataEvent(std::move(req_event2));
  source_->AcceptDataEvent(std::move(resp_event0));
  PL_UNUSED(resp_event1);  // Missing event.
  source_->AcceptDataEvent(std::move(resp_event2));
  source_->AcceptCloseConnEvent(close_conn);

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
  conn_info_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent(kReq0);
  std::unique_ptr<SocketDataEvent> conn0_resp_event = InitRecvEvent(kResp0);
  conn_info_t conn0_close = InitClose();

  conn_info_t conn1 = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> conn1_req_event = InitSendEvent(kReq1);
  std::unique_ptr<SocketDataEvent> conn1_resp_event = InitRecvEvent(kResp1);
  conn_info_t conn1_close = InitClose();

  conn_info_t conn2 = InitConn(TrafficProtocol::kProtocolHTTP);
  std::unique_ptr<SocketDataEvent> conn2_req_event = InitSendEvent(kReq2);
  std::unique_ptr<SocketDataEvent> conn2_resp_event = InitRecvEvent(kResp2);
  conn_info_t conn2_close = InitClose();

  DataTable data_table(kHTTPTable);

  // Simulating scrambled order due to perf buffer, with a couple missing events.
  source_->AcceptDataEvent(std::move(conn0_req_event));
  source_->AcceptOpenConnEvent(conn1);
  source_->AcceptCloseConnEvent(conn2_close);
  source_->AcceptDataEvent(std::move(conn0_resp_event));
  source_->AcceptOpenConnEvent(conn0);
  source_->AcceptDataEvent(std::move(conn2_req_event));
  source_->AcceptDataEvent(std::move(conn1_resp_event));
  source_->AcceptDataEvent(std::move(conn1_req_event));
  source_->AcceptOpenConnEvent(conn2);
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
  conn_info_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  conn_info_t conn0_close = InitClose();

  conn0.traffic_class.protocol = kProtocolUnknown;
  conn0_close.traffic_class.protocol = kProtocolUnknown;

  DataTable data_table(kHTTPTable);

  source_->AcceptOpenConnEvent(conn0);
  source_->AcceptCloseConnEvent(conn0_close);

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

  conn_info_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  conn0.conn_id.pid = impossible_pid;

  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent(kReq0);
  conn0_req_event->attr.conn_id.pid = impossible_pid;

  std::unique_ptr<SocketDataEvent> conn0_resp_event = InitRecvEvent(kResp0);
  conn0_resp_event->attr.conn_id.pid = impossible_pid;

  conn_info_t conn0_close = InitClose();
  conn0_close.conn_id.pid = impossible_pid;

  DataTable data_table(kHTTPTable);

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptOpenConnEvent(conn0);
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

  conn_info_t conn0 = InitConn(TrafficProtocol::kProtocolHTTP);
  conn0.conn_id.pid = real_pid;
  conn0.conn_id.fd = real_fd;

  // An incomplete message means it shouldn't be parseable (we don't want TranfserData to succeed).
  std::unique_ptr<SocketDataEvent> conn0_req_event = InitSendEvent("GET /index.html HTTP/1.1\r\n");
  conn0_req_event->attr.conn_id.pid = real_pid;
  conn0_req_event->attr.conn_id.fd = real_fd;

  DataTable data_table(kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  // Simulating events being emitted from BPF perf buffer.
  source_->AcceptOpenConnEvent(conn0);
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

  // We should find some raw events in send_data.
  EXPECT_TRUE(tracker->recv_data().Empty<http::HTTPMessage>());
  EXPECT_FALSE(tracker->send_data().Empty<http::HTTPMessage>());

  sleep(2);

  // Connection should be timed out by next TransferData,
  // which should also cause events to be flushed.

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

  conn_info_t conn = InitConn(TrafficProtocol::kProtocolMySQL);
  std::unique_ptr<SocketDataEvent> prepare_req_event = InitSendEvent(mySQLStmtPrepareReq);
  std::vector<std::unique_ptr<SocketDataEvent>> prepare_resp_events;
  for (std::string resp_packet : mySQLStmtPrepareResp) {
    prepare_resp_events.push_back(InitRecvEvent(resp_packet));
  }

  std::unique_ptr<SocketDataEvent> execute_req_event = InitSendEvent(mySQLStmtExecuteReq);
  std::vector<std::unique_ptr<SocketDataEvent>> execute_resp_events;
  for (std::string resp_packet : mySQLStmtExecuteResp) {
    execute_resp_events.push_back(InitRecvEvent(resp_packet));
  }

  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(prepare_req_event));
  for (size_t i = 0; i < prepare_resp_events.size(); ++i) {
    source_->AcceptDataEvent(std::move(prepare_resp_events[i]));
  }

  source_->AcceptDataEvent(std::move(execute_req_event));
  for (size_t i = 0; i < execute_resp_events.size(); ++i) {
    source_->AcceptDataEvent(std::move(execute_resp_events[i]));
  }

  DataTable data_table(SocketTraceConnector::kMySQLTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();
  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size());
  }

  std::string expected_entry =
      "{\"Message\": \"SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
      "JOIN sock_tag ON "
      "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
      "GROUP "
      "BY id ORDER BY id\"}";

  // Test execute fail after close. It should create an entry with the Error.
  std::unique_ptr<SocketDataEvent> close_req_event = InitSendEvent(mySQLStmtCloseReq);
  std::unique_ptr<SocketDataEvent> execute_req_event2 = InitSendEvent(mySQLStmtExecuteReq);
  std::unique_ptr<SocketDataEvent> execute_resp_event2 = InitRecvEvent(mySQLErrResp);

  source_->AcceptDataEvent(std::move(close_req_event));
  source_->AcceptDataEvent(std::move(execute_req_event2));
  source_->AcceptDataEvent(std::move(execute_resp_event2));
  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(2, column->Size());
  }

  std::string expected_err = "{\"Error\": \"This an error.\"}";
  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]),
              ElementsAre(expected_entry, expected_err));
}

TEST_F(SocketTraceConnectorTest, MySQLQuery) {
  FLAGS_stirling_enable_mysql_tracing = true;

  conn_info_t conn = InitConn(TrafficProtocol::kProtocolMySQL);
  std::unique_ptr<SocketDataEvent> query_req_event = InitSendEvent(mySQLQueryReq);
  std::vector<std::unique_ptr<SocketDataEvent>> query_resp_events;
  for (std::string resp_packet : mySQLQueryResp) {
    query_resp_events.push_back(InitRecvEvent(resp_packet));
  }

  source_->AcceptOpenConnEvent(conn);
  source_->AcceptDataEvent(std::move(query_req_event));
  for (size_t i = 0; i < query_resp_events.size(); ++i) {
    source_->AcceptDataEvent(std::move(query_resp_events[i]));
  }

  DataTable data_table(SocketTraceConnector::kMySQLTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size());
  }
  std::string expected_entry = "{\"Message\": \"SELECT name FROM tag;\"}";

  EXPECT_THAT(ToStringVector(record_batch[kMySQLRespBodyIdx]), ElementsAre(expected_entry));
}

}  // namespace stirling
}  // namespace pl
