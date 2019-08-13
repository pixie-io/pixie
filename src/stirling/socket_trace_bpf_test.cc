#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <string_view>
#include <thread>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/data_table.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/tcp_socket.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::TCPSocket;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

using SendRecvScript = std::vector<std::vector<std::string_view>>;

class SocketTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    source_ = SocketTraceConnector::Create("socket_trace_connector");
    ASSERT_OK(source_->Init());
    TestOnlySetTargetPID(getpid());
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  class ClientServerSystem {
   public:
    ClientServerSystem() { server_.Bind(); }

    static void WriteData(const TCPSocket& socket, const std::vector<std::string_view>& data) {
      for (auto d : data) {
        ASSERT_EQ(d.length(), socket.Write(d));
      }
    }

    static void ReadData(const TCPSocket& socket, size_t expected_size) {
      std::string msg;
      size_t s = 0;
      while (s < expected_size) {
        socket.Read(&msg);
        s += msg.size();
      }
    }

    static void SendData(const TCPSocket& socket, const std::vector<std::string_view>& data) {
      for (auto d : data) {
        ASSERT_EQ(d.length(), socket.Send(d));
      }
    }

    static void RecvData(const TCPSocket& socket, size_t expected_size) {
      std::string msg;
      size_t s = 0;
      while (s < expected_size) {
        socket.Recv(&msg);
        s += msg.size();
      }
    }

    static void ReadVData(const TCPSocket& socket) {
      std::string msg;
      while (socket.ReadV(&msg) > 0) {
      }
    }

    static void RecvMsg(const TCPSocket& socket) {
      std::vector<std::string> msgs;
      while (socket.RecvMsg(&msgs) > 0) {
      }
    }

    static void WriteVData(const TCPSocket& socket,
                           const std::vector<std::vector<std::string_view>>& write_data) {
      std::string msg;
      for (const auto& data : write_data) {
        socket.WriteV(data);
      }
    }

    static void SendMsg(const TCPSocket& socket,
                        const std::vector<std::vector<std::string_view>>& write_data) {
      std::string msg;
      for (const auto& data : write_data) {
        socket.SendMsg(data);
      }
    }

    /**
     * Run the script in an alternating order, client sends and server receives during even phase,
     * and vice versa.
     * @param socket: client or server socket
     * @param is_client: whether it's the client or server
     * @param ingressFunc: read/recv
     * @param egressFunc: write/send
     */
    void Run(
        const SendRecvScript& script, const TCPSocket& socket, bool is_client,
        std::function<void(const TCPSocket&, size_t)> ingressFunc,
        std::function<void(const TCPSocket&, const std::vector<std::string_view>&)> egressFunc) {
      size_t phase = 0;

      while (phase != script.size()) {
        // phase%2 == 0 |  client   | is_sender
        //      1       |     1     |  1
        //      1       |     0     |  0
        //      0       |     0     |  1
        //      0       |     1     |  0
        if (!((phase % 2 == 0) ^ is_client)) {
          egressFunc(socket, script[phase]);
          phase++;
        } else {
          size_t expected_size = 0;
          for (size_t i = 0; i < script[phase].size(); ++i) {
            expected_size += script[phase][i].size();
          }
          ingressFunc(socket, expected_size);
          phase++;
        }
      }
    }

    void RunWriteRead(const SendRecvScript& script) {
      SpawnClient(script, ReadData, WriteData);
      SpawnServer(script, ReadData, WriteData);
      JoinThreads();
    }

    void RunSendRecv(const SendRecvScript& script) {
      SpawnClient(script, RecvData, SendData);
      SpawnServer(script, RecvData, SendData);
      JoinThreads();
    }

    void RunSendMsgRecvMsg(const std::vector<std::vector<std::string_view>>& write_data) {
      SpawnMsgClient(RecvMsg);
      SpawnMsgServer(write_data, SendMsg);
      JoinThreads();
    }

    void RunWriteVReadV(const std::vector<std::vector<std::string_view>>& write_data) {
      SpawnMsgClient(ReadVData);
      SpawnMsgServer(write_data, WriteVData);
      JoinThreads();
    }

    void SpawnClient(
        const SendRecvScript& script, std::function<void(const TCPSocket&, size_t)> ingressFunc,
        std::function<void(const TCPSocket&, const std::vector<std::string_view>&)> egressFunc) {
      client_thread_ = std::thread([this, script, ingressFunc, egressFunc]() {
        client_.Connect(server_);
        Run(script, client_, true, ingressFunc, egressFunc);
        client_.Close();
      });
    }

    void SpawnServer(
        const SendRecvScript& script, std::function<void(const TCPSocket&, size_t)> ingressFunc,
        std::function<void(const TCPSocket&, const std::vector<std::string_view>&)> egressFunc) {
      server_thread_ = std::thread([this, script, ingressFunc, egressFunc]() {
        server_.Accept();
        Run(script, server_, false, ingressFunc, egressFunc);
        server_.Close();
      });
    }

    // TODO(chengruizhe): Currently SendMsg/RecvMsg & WriteV/ReadV doesn't follow the script and
    // multi-round communiation model. Fix this.
    void SpawnMsgServer(
        const std::vector<std::vector<std::string_view>>& write_data,
        std::function<void(const TCPSocket&, const std::vector<std::vector<std::string_view>>&)>
            egressFunc) {
      server_thread_ = std::thread([this, write_data, egressFunc]() {
        server_.Accept();
        egressFunc(server_, write_data);
        server_.Close();
      });
    }

    void SpawnMsgClient(std::function<void(const TCPSocket&)> ingressFunc) {
      client_thread_ = std::thread([this, ingressFunc]() {
        client_.Connect(server_);
        ingressFunc(client_);
        client_.Close();
      });
    }

    void JoinThreads() {
      server_thread_.join();
      client_thread_.join();
    }

    TCPSocket& Server() { return server_; }
    TCPSocket& Client() { return client_; }

   private:
    TCPSocket client_;
    TCPSocket server_;

    std::thread client_thread_;
    std::thread server_thread_;
  };

  std::string kStmtPrepareReq;
  std::vector<std::string> StmtPrepareResp;
  std::string kStmtExecuteReq;
  std::vector<std::string> StmtExecuteResp;
  std::string kQueryReq;
  std::vector<std::string> QueryResp;

  SendRecvScript GetPrepareExecuteScript() {
    SendRecvScript prepare_execute_script;

    // Stmt Prepare
    kStmtPrepareReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStringRequest(mysql::testutils::kStmtPrepareRequest,
                                              mysql::MySQLEventType::kComStmtPrepare)
               .msg);
    prepare_execute_script.push_back({kStmtPrepareReq});
    prepare_execute_script.push_back({});
    std::deque<mysql::Packet> prepare_packets =
        mysql::testutils::GenStmtPrepareOKResponse(mysql::testutils::kStmtPrepareResponse);
    for (int i = 0; i < static_cast<int>(prepare_packets.size()); ++i) {
      // i + 1 because packet_num 0 is the request, so response starts from 1.
      StmtPrepareResp.push_back(mysql::testutils::GenRawPacket(i + 1, prepare_packets[i].msg));
    }
    for (size_t i = 0; i < StmtPrepareResp.size(); ++i) {
      prepare_execute_script[1].push_back(StmtPrepareResp[i]);
    }

    // Stmt Execute
    kStmtExecuteReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStmtExecuteRequest(mysql::testutils::kStmtExecuteRequest).msg);
    prepare_execute_script.push_back({kStmtExecuteReq});
    prepare_execute_script.push_back({});
    std::deque<mysql::Packet> execute_packets =
        mysql::testutils::GenResultset(mysql::testutils::kStmtExecuteResultset);
    for (int i = 0; i < static_cast<int>(execute_packets.size()); ++i) {
      // i + 1 because packet_num 0 is the request, so response starts from 1.
      StmtExecuteResp.push_back(mysql::testutils::GenRawPacket(i + 1, execute_packets[i].msg));
    }
    for (size_t i = 0; i < StmtExecuteResp.size(); ++i) {
      prepare_execute_script[3].push_back(StmtExecuteResp[i]);
    }
    return prepare_execute_script;
  }

  SendRecvScript GetQueryScript() {
    SendRecvScript query_script;

    kQueryReq = mysql::testutils::GenRawPacket(
        0, mysql::testutils::GenStringRequest(mysql::testutils::kQueryRequest,
                                              mysql::MySQLEventType::kComQuery)
               .msg);
    query_script.push_back({kQueryReq});
    query_script.push_back({});
    std::deque<mysql::Packet> query_packets =
        mysql::testutils::GenResultset(mysql::testutils::kQueryResultset);
    for (int i = 0; i < static_cast<int>(query_packets.size()); ++i) {
      // i + 1 because packet_num 0 is the request, so response starts from 1.
      QueryResp.push_back(mysql::testutils::GenRawPacket(i + 1, query_packets[i].msg));
    }
    for (size_t i = 0; i < QueryResp.size(); ++i) {
      query_script[1].push_back(QueryResp[i]);
    }
    return query_script;
  }

  void ConfigureCapture(TrafficProtocol protocol, uint64_t mask) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->Configure(protocol, mask));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID(pid));
  }

  static constexpr std::string_view kHTTPReqMsg1 = R"(GET /endpoint1 HTTP/1.1
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0

)";

  static constexpr std::string_view kHTTPReqMsg2 = R"(GET /endpoint2 HTTP/1.1
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0

)";

  static constexpr std::string_view kHTTPRespMsg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg1
Content-Length: 0

)";

  static constexpr std::string_view kHTTPRespMsg2 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg2
Content-Length: 0

)";

  static constexpr std::string_view kNoProtocolMsg = R"(This is not an HTTP message)";

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr DataTableSchema kHTTPTable = SocketTraceConnector::kHTTPTable;
  static constexpr uint32_t kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
  static constexpr uint32_t kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
  static constexpr uint32_t kHTTPRespStatusIdx = kHTTPTable.ColIndex("http_resp_status");
  static constexpr uint32_t kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");
  static constexpr uint32_t kHTTPRespMessageIdx = kHTTPTable.ColIndex("http_resp_message");
  static constexpr uint32_t kHTTPRespHeadersIdx = kHTTPTable.ColIndex("http_resp_headers");
  static constexpr uint32_t kHTTPPIDIdx = kHTTPTable.ColIndex("pid");
  static constexpr uint32_t kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
  static constexpr uint32_t kHTTPStartTimeIdx = kHTTPTable.ColIndex("pid_start_time");

  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;
  static constexpr DataTableSchema kMySQLTable = SocketTraceConnector::kMySQLTable;
  static constexpr uint32_t kMySQLBodyIdx = kMySQLTable.ColIndex("body");

  std::unique_ptr<SourceConnector> source_;
};

TEST_F(SocketTraceBPFTest, TestFramework) {
  ConfigureCapture(kProtocolHTTP, kRoleRequestor | kRoleResponder);

  SendRecvScript script({
      {kHTTPReqMsg1},
      {kHTTPRespMsg1},
      {kHTTPReqMsg2},
      {kHTTPRespMsg2},
  });
  ClientServerSystem system;
  system.RunWriteRead(script);

  DataTable data_table(kHTTPTable);
  source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    ASSERT_EQ(4, col->Size());
  }
}

// TODO(chengruizhe): Add test targeted at checking IPs.

TEST_F(SocketTraceBPFTest, TestWriteRespCapture) {
  ConfigureCapture(kProtocolHTTP, kRoleResponder);

  ClientServerSystem system;
  system.RunWriteRead({{kHTTPRespMsg1, kHTTPRespMsg2}});

  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These getpid() EXPECTs require docker container with --pid=host so that the container's PID
    // and the host machine are identical. See
    // https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(1));

    // Additional verifications. These are common to all HTTP1.x tracing, so we decide to not
    // duplicate them on all relevant tests.
    EXPECT_EQ(1, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(1, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(1).val);
  }

  // Check that MySQL table did not capture any data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(/* ctx */ nullptr, kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestSendRespCapture) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleResponder);

  ClientServerSystem system;
  system.RunSendRecv({{kHTTPRespMsg1, kHTTPRespMsg2}});

  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(1));
  }

  // Check that MySQL table did not capture any data.
  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestReadRespCapture) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  ClientServerSystem system;
  system.RunWriteRead({{kHTTPRespMsg1, kHTTPRespMsg2}});

  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(1));
  }

  // Check that MySQL table did not capture any data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(/* ctx */ nullptr, kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestRecvRespCapture) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  ClientServerSystem system;
  system.RunSendRecv({{kHTTPRespMsg1, kHTTPRespMsg2}});

  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(1));
  }

  // Check that MySQL table did not capture any data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(/* ctx */ nullptr, kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestEnd2EndMySQLPrepareExecute) {
  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kRoleRequestor);
  ClientServerSystem system;
  system.RunWriteRead(GetPrepareExecuteScript());

  // Check that HTTP table did not capture any data.
  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did capture the appropriate data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(/* ctx */ nullptr, kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(1, col->Size());
    }

    EXPECT_EQ(
        "{\"Message\": \"SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM "
        "sock "
        "JOIN sock_tag ON "
        "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
        "GROUP "
        "BY id ORDER BY id\"}",
        record_batch[kMySQLBodyIdx]->Get<types::StringValue>(0));
  }
}

TEST_F(SocketTraceBPFTest, TestEnd2EndMySQLQuery) {
  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kRoleRequestor);
  ClientServerSystem system;
  system.RunWriteRead(GetQueryScript());

  // Check that HTTP table did not capture any data.
  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did capture the appropriate data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(/* ctx */ nullptr, kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(1, col->Size());
    }

    EXPECT_EQ("{\"Message\": \"SELECT name FROM tag;\"}",
              record_batch[kMySQLBodyIdx]->Get<types::StringValue>(0));
  }
}

TEST_F(SocketTraceBPFTest, TestNoProtocolWritesNotCaptured) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor | kRoleResponder);
  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kRoleRequestor);

  ClientServerSystem system;
  system.RunWriteRead({{kNoProtocolMsg, "", kNoProtocolMsg, ""}});

  // Check that HTTP table did not capture any data.
  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    // Should not have captured anything.
    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did not capture any data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(/* ctx */ nullptr, kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    // Should not have captured anything.
    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestMultipleConnections) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  // Two separate connections.
  ClientServerSystem system1;
  system1.RunWriteRead({{kHTTPRespMsg1}});

  ClientServerSystem system2;
  system2.RunWriteRead({{kHTTPRespMsg2}});

  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    std::vector<std::tuple<int64_t, std::string>> results;
    for (int i = 0; i < 2; ++i) {
      results.emplace_back(
          std::make_tuple(record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(i).val,
                          record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(i)));
    }

    EXPECT_THAT(
        results,
        UnorderedElementsAre(
            std::make_tuple(getpid(), "Content-Length: 0\nContent-Type: application/json; msg1"),
            std::make_tuple(getpid(), "Content-Length: 0\nContent-Type: application/json; msg2")));
  }
}

TEST_F(SocketTraceBPFTest, TestStartTime) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  ClientServerSystem system;
  system.RunSendRecv({{kHTTPRespMsg1, kHTTPRespMsg2}});

  // Kernel uses monotonic clock as start_time, so we must do the same.
  auto now = std::chrono::steady_clock::now();

  // Use a time window to make sure the recorded PID start_time is right.
  // Being super generous with the window, just in case test runs slow.
  auto time_window_start = now - std::chrono::minutes(30);
  auto time_window_end = now + std::chrono::minutes(5);

  DataTable data_table(kHTTPTable);
  source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  ASSERT_EQ(2, record_batch[0]->Size());

  EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
  EXPECT_LT(time_window_start.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(0).val);
  EXPECT_GT(time_window_end.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(0).val);

  EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
  EXPECT_LT(time_window_start.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(1).val);
  EXPECT_GT(time_window_end.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(1).val);
}

// TODO(yzhao): Apply this pattern to other syscall pairs. An issue is that other syscalls do not
// use scatter buffer. One approach would be to concatenate inner vector to a single string, and
// then feed to the syscall. Another caution is that value-parameterized tests actually discourage
// changing functions being tested according to test parameters. The canonical pattern is using test
// parameters as inputs, but keep the function being tested fixed.
enum class SyscallPair {
  kSendRecvMsg,
  kWriteReadv,
};

class SyscallPairBPFTest : public SocketTraceBPFTest,
                           public ::testing::WithParamInterface<SyscallPair> {};

TEST_P(SyscallPairBPFTest, EventsAreCaptured) {
  ConfigureCapture(kProtocolHTTP, kRoleRequestor | kRoleResponder);
  ClientServerSystem system({});
  const std::vector<std::vector<std::string_view>> data = {
      {"HTTP/1.1 200 OK\r\n", "Content-Type: json\r\n", "Content-Length: 1\r\n\r\na"},
      {"HTTP/1.1 404 Not Found\r\n", "Content-Type: json\r\n", "Content-Length: 2\r\n\r\nbc"}};
  switch (GetParam()) {
    case SyscallPair::kSendRecvMsg:
      system.RunSendMsgRecvMsg(data);
      break;
    case SyscallPair::kWriteReadv:
      system.RunWriteVReadV(data);
      break;
  }

  DataTable data_table(kHTTPTable);
  source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    // 2 for sendmsg() and 2 for recvmsg().
    ASSERT_EQ(4, col->Size());
  }

  for (int i : {0, 2}) {
    EXPECT_THAT(std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(i)),
                StrEq("Content-Length: 1\nContent-Type: json"));
    EXPECT_EQ(200, record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(i).val);
    EXPECT_THAT(std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(i)),
                StrEq("a"));
    EXPECT_THAT(std::string(record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(i)),
                StrEq("OK"));
  }
  for (int i : {1, 3}) {
    EXPECT_THAT(std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(i)),
                StrEq("Content-Length: 2\nContent-Type: json"));
    EXPECT_EQ(404, record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(i).val);
    EXPECT_THAT(std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(i)),
                StrEq("bc"));
    EXPECT_THAT(std::string(record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(i)),
                StrEq("Not Found"));
  }
}

INSTANTIATE_TEST_CASE_P(IOVecSyscalls, SyscallPairBPFTest,
                        ::testing::Values(SyscallPair::kSendRecvMsg, SyscallPair::kWriteReadv));

}  // namespace stirling
}  // namespace pl
