#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/client_server_system.h"

namespace pl {
namespace stirling {
namespace testing {

// TODO(yzhao): Move MySQL-specific code into src/stirling/mysql_trace_bpf_test.cc.
class SocketTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_stirling_disable_self_tracing = false;

    source_ = SocketTraceConnector::Create("socket_trace_connector");
    ASSERT_OK(source_->Init());

    // Create a context to pass into each TransferData() in the test, using a dummy ASID.
    static constexpr uint32_t kASID = 1;
    auto agent_metadata_state = std::make_shared<md::AgentMetadataState>(kASID);
    ctx_ = std::make_unique<ConnectorContext>(std::move(agent_metadata_state));

    TestOnlySetTargetPID(getpid());
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  std::string stmt_prepare_req;
  std::vector<std::string> stmt_prepare_resp;
  std::string stmt_execute_req;
  std::vector<std::string> stmt_execute_resp;
  std::string stmt_close_req;
  std::string query_req;
  std::vector<std::string> query_resp;

  testing::SendRecvScript GetPrepareExecuteScript() {
    testing::SendRecvScript prepare_execute_script;

    // Stmt Prepare
    stmt_prepare_req = mysql::testutils::GenRawPacket(mysql::testutils::GenStringRequest(
        mysql::testdata::kStmtPrepareRequest, mysql::MySQLEventType::kStmtPrepare));
    prepare_execute_script.push_back({stmt_prepare_req});
    prepare_execute_script.push_back({});
    std::deque<mysql::Packet> prepare_packets =
        mysql::testutils::GenStmtPrepareOKResponse(mysql::testdata::kStmtPrepareResponse);
    for (const auto& prepare_packet : prepare_packets) {
      stmt_prepare_resp.push_back(mysql::testutils::GenRawPacket(prepare_packet));
    }
    for (size_t i = 0; i < stmt_prepare_resp.size(); ++i) {
      prepare_execute_script[1].push_back(stmt_prepare_resp[i]);
    }

    // Stmt Execute
    stmt_execute_req = mysql::testutils::GenRawPacket(
        mysql::testutils::GenStmtExecuteRequest(mysql::testdata::kStmtExecuteRequest));
    prepare_execute_script.push_back({stmt_execute_req});
    prepare_execute_script.push_back({});
    std::deque<mysql::Packet> execute_packets =
        mysql::testutils::GenResultset(mysql::testdata::kStmtExecuteResultset);
    for (const auto& execute_packet : execute_packets) {
      stmt_execute_resp.push_back(mysql::testutils::GenRawPacket(execute_packet));
    }
    for (size_t i = 0; i < stmt_execute_resp.size(); ++i) {
      prepare_execute_script[3].push_back(stmt_execute_resp[i]);
    }

    // Stmt Close
    stmt_close_req = mysql::testutils::GenRawPacket(
        mysql::testutils::GenStmtCloseRequest(mysql::testdata::kStmtCloseRequest));
    prepare_execute_script.push_back({stmt_close_req});
    prepare_execute_script.push_back({});

    return prepare_execute_script;
  }

  testing::SendRecvScript GetQueryScript() {
    testing::SendRecvScript query_script;

    query_req = mysql::testutils::GenRawPacket(mysql::testutils::GenStringRequest(
        mysql::testdata::kQueryRequest, mysql::MySQLEventType::kQuery));
    query_script.push_back({query_req});
    query_script.push_back({});
    std::deque<mysql::Packet> query_packets =
        mysql::testutils::GenResultset(mysql::testdata::kQueryResultset);
    for (const auto& query_packet : query_packets) {
      query_resp.push_back(mysql::testutils::GenRawPacket(query_packet));
    }
    for (size_t i = 0; i < query_resp.size(); ++i) {
      query_script[1].push_back(query_resp[i]);
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

  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;
  static constexpr uint32_t kMySQLReqBodyIdx = kMySQLTable.ColIndex("req_body");

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<ConnectorContext> ctx_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
