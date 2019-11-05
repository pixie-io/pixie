#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::stirling::testing::TCPSocket;
using ::pl::types::ColumnWrapper;

TEST_F(SocketTraceBPFTest, MySQLStmtPrepareExecuteClose) {
  FLAGS_stirling_enable_mysql_tracing = true;

  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kRoleRequestor);
  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(GetPrepareExecuteScript());

  // Check that HTTP table did not capture any data.
  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did capture the appropriate data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(3, col->Size());
    }

    EXPECT_EQ(
        "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM "
        "sock "
        "JOIN sock_tag ON "
        "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=? "
        "GROUP "
        "BY id ORDER BY ?",
        record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(
        "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM "
        "sock "
        "JOIN sock_tag ON "
        "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
        "GROUP "
        "BY id ORDER BY id",
        record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(1));

    EXPECT_EQ("", record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(2));
  }
}

TEST_F(SocketTraceBPFTest, MySQLQuery) {
  FLAGS_stirling_enable_mysql_tracing = true;

  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kRoleRequestor);
  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(GetQueryScript());

  // Check that HTTP table did not capture any data.
  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did capture the appropriate data.
  {
    DataTable data_table(kMySQLTable);
    source_->TransferData(ctx_.get(), kMySQLTableNum, &data_table);
    types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(1, col->Size());
    }

    EXPECT_EQ("SELECT name FROM tag;", record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(0));
  }
}

}  // namespace stirling
}  // namespace pl
