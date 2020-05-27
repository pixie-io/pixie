#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/conn_stats_table.h"
#include "src/stirling/output.h"
#include "src/stirling/testing/client_server_system.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::AccessRecordBatch;
using ::pl::stirling::testing::ClientServerSystem;
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::SendRecvScript;
using ::pl::stirling::testing::TCPSocket;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class ConnStatsBPFTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ true> {
 protected:
  DataTable data_table_{kConnStatsTable};
};

TEST_F(ConnStatsBPFTest, UnclassifiedEvents) {
  ClientServerSystem cs;

  SendRecvScript script;
  script.push_back({{"req1"}, {"resp1"}});
  script.push_back({{"req2"}, {"resp2"}});
  cs.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script);

  source_->TransferData(ctx_.get(), SocketTraceConnector::kConnStatsTableNum, &data_table_);
  const types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();
  PrintRecordBatch("test", kConnStatsTable.elements(), record_batch);

  auto indices = FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, getpid());
  ASSERT_THAT(indices, SizeIs(2));

  std::vector<int> bytes_sent_vals;
  std::vector<int> bytes_recv_vals;
  for (auto i : indices) {
    bytes_sent_vals.push_back(
        AccessRecordBatch<types::Int64Value>(record_batch, conn_stats_idx::kBytesSent, i).val);
    bytes_recv_vals.push_back(
        AccessRecordBatch<types::Int64Value>(record_batch, conn_stats_idx::kBytesRecv, i).val);
  }
  EXPECT_THAT(bytes_sent_vals, UnorderedElementsAre(8, 10));
  EXPECT_THAT(bytes_recv_vals, UnorderedElementsAre(8, 10));
}

// Test fixture that starts SocketTraceConnector after the connection was already established.
class ConnStatsMidConnBPFTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ true> {
 protected:
  void SetUp() override {
    server_.BindAndListen();

    // Server reads one message and exists.
    // The client will write one message in the test body, after the SocketTraceConnector is
    // created.
    server_thread_ = std::thread([this]() {
      auto socket = server_.Accept();
      std::string text;
      CHECK(socket->Read(&text));
      socket->Close();
    });

    client_.Connect(server_);

    // Now SocketTraceConnector is created after the connection was already established.
    SocketTraceBPFTest::SetUp();
  }

  void TearDown() override {
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
    SocketTraceBPFTest::TearDown();
  }

  TCPSocket server_;
  std::thread server_thread_;

  TCPSocket client_;
  DataTable data_table_{kConnStatsTable};
};

TEST_F(ConnStatsMidConnBPFTest, DidNotSeeConnEstablishment) {
  std::string_view test_msg = "Hello World!";
  EXPECT_EQ(test_msg.size(), client_.Write(test_msg));

  source_->TransferData(ctx_.get(), SocketTraceConnector::kConnStatsTableNum, &data_table_);
  const types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();

  auto indices = FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, getpid());
  ASSERT_THAT(indices, IsEmpty());
}

}  // namespace stirling
}  // namespace pl
