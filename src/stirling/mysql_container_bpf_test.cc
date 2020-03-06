#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/data_table.h"
#include "src/stirling/mysql/types.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::FindRecordIdxMatchesPid;
using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class MySQLContainer : public ContainerRunner {
 public:
  MySQLContainer() : ContainerRunner(kMySQLImage, kMySQLInstanceNamePrefix, kMySQLReadyMessage) {}

 private:
  static constexpr std::string_view kMySQLImage = "mysql/mysql-server:8.0.13";
  static constexpr std::string_view kMySQLInstanceNamePrefix = "mysql_server";
  static constexpr std::string_view kMySQLReadyMessage =
      "/usr/sbin/mysqld: ready for connections. Version: '8.0.13'  socket: "
      "'/var/lib/mysql/mysql.sock'  port: 3306";
};

class MySQLTraceTest : public SocketTraceBPFTest {
 protected:
  MySQLTraceTest() {
    std::string script_path =
        TestEnvironment::PathToTestDataFile("src/stirling/mysql/testing/script.sql");
    LOG(INFO) << script_path;

    // Run the MySQL server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to download the MySQL image.
    StatusOr<std::string> run_result = container_.Run(90, {"--env=MYSQL_ALLOW_EMPTY_PASSWORD=1"});
    PL_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }
  ~MySQLTraceTest() { container_.Stop(); }

  MySQLContainer container_;
};

//-----------------------------------------------------------------------------
// Utility Functions and Matchers
//-----------------------------------------------------------------------------

std::vector<mysql::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                          const std::vector<size_t>& indices) {
  std::vector<mysql::Record> result;

  for (const auto& idx : indices) {
    mysql::Record r;
    r.req.cmd =
        static_cast<mysql::MySQLEventType>(rb[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx).val);
    r.req.msg = rb[kMySQLReqBodyIdx]->Get<types::StringValue>(idx);
    r.resp.status = static_cast<mysql::MySQLRespStatus>(
        rb[kMySQLRespStatusIdx]->Get<types::Int64Value>(idx).val);
    r.resp.msg = rb[kMySQLRespBodyIdx]->Get<types::StringValue>(idx);
    result.push_back(r);
  }
  return result;
}

auto EqMySQLReq(const mysql::MySQLRequest& x) {
  return AllOf(Field(&mysql::MySQLRequest::cmd, Eq(x.cmd)),
               Field(&mysql::MySQLRequest::msg, StrEq(x.msg)));
}

auto EqMySQLResp(const mysql::MySQLResponse& x) {
  return AllOf(Field(&mysql::MySQLResponse::status, Eq(x.status)),
               Field(&mysql::MySQLResponse::msg, StrEq(x.msg)));
}

auto EqMySQLRecord(const mysql::Record& x) {
  return AllOf(Field(&mysql::Record::req, EqMySQLReq(x.req)),
               Field(&mysql::Record::resp, EqMySQLResp(x.resp)));
}

//-----------------------------------------------------------------------------
// Expected Test Data
//-----------------------------------------------------------------------------

// clang-format off
mysql::Record kRecord1 = {
  .req = {
          .cmd = mysql::MySQLEventType::kQuery,
          .msg = R"(select @@version_comment limit 1)",
          .timestamp_ns = 0,
  },
  .resp = {
          .status = mysql::MySQLRespStatus::kOK,
          .msg = R"(Resultset rows = 1)",
          .timestamp_ns = 0,
  }
};

mysql::Record kRecord2 = {
  .req = {
          .cmd = mysql::MySQLEventType::kQuery,
          .msg = R"(select table_schema as database_name, table_name from information_schema.tables)",
          .timestamp_ns = 0,
  },
  .resp = {
          .status = mysql::MySQLRespStatus::kOK,
          .msg = R"(Resultset rows = 301)",
          .timestamp_ns = 0,
  }
};

mysql::Record kRecord3 = {
  .req = {
      .cmd = mysql::MySQLEventType::kQuery,
      .msg = R"(quit)",
      .timestamp_ns = 0,
  },
  .resp = {
      .status = mysql::MySQLRespStatus::kErr,
      .msg = R"(You have an error in your SQL syntax;"
" check the manual that corresponds to your MySQL server version"
" for the right syntax to use near 'quit' at line 1)",
      .timestamp_ns = 0,
  }
};
// clang-format on

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(MySQLTraceTest, mysql_capture) {
  std::string script_path =
      TestEnvironment::PathToTestDataFile("src/stirling/mysql/testing/script.sql");
  ASSERT_OK_AND_ASSIGN(std::string script_content, pl::ReadFileToString(script_path));

  // Run mysql as a way of generating traffic.
  // Run it through bash, and return the PID, so we can use it to filter captured results.
  std::string cmd =
      absl::StrFormat("docker exec %s bash -c 'echo \"%s\" | mysql -uroot & echo $! && wait'",
                      container_.container_name(), script_content);
  LOG(INFO) << cmd;
  ASSERT_OK_AND_ASSIGN(std::string out, pl::Exec(cmd));

  std::vector<std::string_view> lines = absl::StrSplit(out, "\n");
  ASSERT_FALSE(lines.empty());

  int32_t client_pid;
  ASSERT_TRUE(absl::SimpleAtoi(lines[0], &client_pid));

  // Sleep a little more, just to be safe.
  sleep(1);

  // Grab the data from Stirling.
  DataTable data_table(kMySQLTable);
  source_->TransferData(ctx_.get(), SocketTraceConnector::kMySQLTableNum, &data_table);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  // Check client-side tracing results.
  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPid(record_batch, kMySQLUPIDIdx, client_pid);

    // For Debug:
    for (const auto& idx : target_record_indices) {
      uint32_t pid = record_batch[kMySQLUPIDIdx]->Get<types::UInt128Value>(idx).High64();
      std::string req_body = record_batch[kMySQLReqBodyIdx]->Get<types::StringValue>(idx);
      std::string resp_body = record_batch[kMySQLRespBodyIdx]->Get<types::StringValue>(idx);
      LOG(INFO) << absl::Substitute("$0 $1 $2", pid, req_body, resp_body);
    }

    std::vector<mysql::Record> records = ToRecordVector(record_batch, target_record_indices);

    EXPECT_THAT(records, UnorderedElementsAre(EqMySQLRecord(kRecord1), EqMySQLRecord(kRecord2)));
  }

  // TODO(oazizi): Check server-side tracing results.
}

}  // namespace stirling
}  // namespace pl
