#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include <absl/strings/str_replace.h>
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
#include "src/stirling/testing/test_output_generator/test_utils.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::testing::TestFilePath;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

DEFINE_bool(tracing_mode, false, "If true, only runs the containers and exits. For tracing.");

class MySQLContainer : public ContainerRunner {
 public:
  MySQLContainer() : ContainerRunner(kImage, kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImage = "mysql/mysql-server:8.0.13";
  static constexpr std::string_view kInstanceNamePrefix = "mysql_server";
  static constexpr std::string_view kReadyMessage =
      "/usr/sbin/mysqld: ready for connections. Version: '8.0.13'  socket: "
      "'/var/lib/mysql/mysql.sock'  port: 3306";
};

class MySQLTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ true> {
 protected:
  MySQLTraceTest() {
    std::string script_path = TestFilePath("src/stirling/mysql/testing/script.sql");
    LOG(INFO) << script_path;

    // Run the MySQL server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to download the MySQL image.
    StatusOr<std::string> run_result = container_.Run(
        90, {"--env=MYSQL_ALLOW_EMPTY_PASSWORD=1", "--env=MYSQL_ROOT_HOST=%", "-p=33060:3306"});
    PL_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }
  ~MySQLTraceTest() { container_.Stop(); }

  StatusOr<int32_t> RunSQLScript(std::string_view script_path) {
    std::string absl_script_path = TestFilePath(script_path);
    PL_ASSIGN_OR_RETURN(std::string script_content, pl::ReadFileToString(absl_script_path));

    // Since script content will be passed through bash, escape any single quotes in the script.
    script_content = absl::StrReplaceAll(script_content, {{"'", "'\\''"}});

    // Run mysql as a way of generating traffic.
    // Run it through bash, and return the PID, so we can use it to filter captured results.
    std::string cmd = absl::StrFormat(
        "docker exec %s bash -c 'echo \"%s\" | mysql --protocol=TCP --ssl-mode=DISABLED "
        "--host=localhost --port=3306 -uroot & echo $! && wait'",
        container_.container_name(), script_content);
    PL_ASSIGN_OR_RETURN(std::string out, pl::Exec(cmd));

    std::vector<std::string_view> lines = absl::StrSplit(out, "\n");
    if (lines.empty()) {
      return error::Internal("Exected output (pid) from command.");
    }

    int32_t client_pid;
    if (!absl::SimpleAtoi(lines[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    LOG(INFO) << absl::Substitute("Client PID: $0", client_pid);

    return client_pid;
  }

  StatusOr<int32_t> RunPythonScript(std::string_view script_path) {
    std::string absl_script_path = TestFilePath(script_path);

    // TODO(chengruizhe): Pull out pip3 install into the environment.
    std::string cmd =
        absl::StrFormat("pip3 install -q mysql-connector-python && python3 %s", absl_script_path);
    PL_ASSIGN_OR_RETURN(std::string out, pl::Exec(cmd));

    std::vector<std::string_view> lines = absl::StrSplit(out, "\n");
    if (lines.empty()) {
      return error::Internal("Exected output (pid) from command.");
    }

    int32_t client_pid;
    if (!absl::SimpleAtoi(lines[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    return client_pid;
  }

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
    r.req.cmd = static_cast<mysql::Command>(rb[kMySQLReqCmdIdx]->Get<types::Int64Value>(idx).val);
    r.req.msg = rb[kMySQLReqBodyIdx]->Get<types::StringValue>(idx);
    r.resp.status =
        static_cast<mysql::RespStatus>(rb[kMySQLRespStatusIdx]->Get<types::Int64Value>(idx).val);
    r.resp.msg = rb[kMySQLRespBodyIdx]->Get<types::StringValue>(idx);
    result.push_back(r);
  }
  return result;
}

auto EqMySQLReq(const mysql::Request& x) {
  return AllOf(Field(&mysql::Request::cmd, Eq(x.cmd)), Field(&mysql::Request::msg, StrEq(x.msg)));
}

auto EqMySQLResp(const mysql::Response& x) {
  return AllOf(Field(&mysql::Response::status, Eq(x.status)),
               Field(&mysql::Response::msg, StrEq(x.msg)));
}

auto EqMySQLRecord(const mysql::Record& x) {
  return AllOf(Field(&mysql::Record::req, EqMySQLReq(x.req)),
               Field(&mysql::Record::resp, EqMySQLResp(x.resp)));
}

//-----------------------------------------------------------------------------
// Expected Test Data
//-----------------------------------------------------------------------------

// clang-format off
mysql::Record kRecordInit = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = R"(select @@version_comment limit 1)",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = R"(Resultset rows = 1)",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript1Cmd1 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = R"(select table_schema as database_name, table_name from information_schema.tables)",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = R"(Resultset rows = 301)",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript1Cmd2 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "SHOW DATABASES",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "Resultset rows = 4",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript1Cmd3 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "SELECT DATABASE()",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "Resultset rows = 1",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript1Cmd4 = {
  .req = {
    .cmd = mysql::Command::kInitDB,
    .msg = "mysql",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript1Cmd5 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "SELECT user, host FROM user",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "Resultset rows = 6",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript1Cmd6 = {
  .req = {
    .cmd = mysql::Command::kQuit,
    .msg = "",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kNone,
    .msg = "",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd1 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = R"(PREPARE pstmt FROM 'select table_schema as database_name, table_name
from information_schema.tables
where table_type = ? and table_schema = ?
order by database_name, table_name')",
    .timestamp_ns = 0,
  },
  .resp = {
      .status = mysql::RespStatus::kOK,
      .msg = "",
      .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd2 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "SET @tableType = 'BASE TABLE'",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd3 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "SET @tableSchema = 'mysql'",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd4 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "EXECUTE pstmt USING @tableType, @tableSchema",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "Resultset rows = 33",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd5 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "SET @tableSchema = 'bogus'",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd6 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "EXECUTE pstmt USING @tableType, @tableSchema",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "Resultset rows = 0",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd7 = {
  .req = {
    .cmd = mysql::Command::kQuery,
    .msg = "DEALLOCATE PREPARE pstmt",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kOK,
    .msg = "",
    .timestamp_ns = 0,
  }
};

mysql::Record kRecordScript2Cmd8 = {
  .req = {
    .cmd = mysql::Command::kQuit,
    .msg = "",
    .timestamp_ns = 0,
  },
  .resp = {
    .status = mysql::RespStatus::kNone,
    .msg = "",
    .timestamp_ns = 0,
  }
};

// clang-format on

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

std::vector<mysql::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                            int32_t client_pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kMySQLUPIDIdx, client_pid);
  return ToRecordVector(record_batch, target_record_indices);
}

TEST_F(MySQLTraceTest, mysql_capture) {
  {
    ASSERT_OK_AND_ASSIGN(int32_t client_pid, RunSQLScript("src/stirling/mysql/testing/script.sql"));

    // Grab the data from Stirling.
    DataTable data_table(kMySQLTable);
    source_->TransferData(ctx_.get(), SocketTraceConnector::kMySQLTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    ASSERT_FALSE(tablets.empty());
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    // Check client-side tracing results.
    if (!FLAGS_tracing_mode) {
      std::vector<mysql::Record> records = GetTargetRecords(record_batch, client_pid);

      EXPECT_THAT(records, UnorderedElementsAre(
                               EqMySQLRecord(kRecordInit), EqMySQLRecord(kRecordScript1Cmd1),
                               EqMySQLRecord(kRecordScript1Cmd2), EqMySQLRecord(kRecordScript1Cmd3),
                               EqMySQLRecord(kRecordScript1Cmd4), EqMySQLRecord(kRecordScript1Cmd5),
                               EqMySQLRecord(kRecordScript1Cmd6)));
    }

    // TODO(oazizi): Check server-side tracing results.
  }

  {
    ASSERT_OK_AND_ASSIGN(int32_t client_pid,
                         RunSQLScript("src/stirling/mysql/testing/prepare_execute.sql"));

    // Grab the data from Stirling.
    DataTable data_table(kMySQLTable);
    source_->TransferData(ctx_.get(), SocketTraceConnector::kMySQLTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    ASSERT_FALSE(tablets.empty());
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    // Check client-side tracing results.
    if (!FLAGS_tracing_mode) {
      std::vector<mysql::Record> records = GetTargetRecords(record_batch, client_pid);

      EXPECT_THAT(records, UnorderedElementsAre(
                               EqMySQLRecord(kRecordInit), EqMySQLRecord(kRecordScript2Cmd1),
                               EqMySQLRecord(kRecordScript2Cmd2), EqMySQLRecord(kRecordScript2Cmd3),
                               EqMySQLRecord(kRecordScript2Cmd4), EqMySQLRecord(kRecordScript2Cmd5),
                               EqMySQLRecord(kRecordScript2Cmd6), EqMySQLRecord(kRecordScript2Cmd7),
                               EqMySQLRecord(kRecordScript2Cmd8)));
    }

    // TODO(oazizi): Check server-side tracing results.
  }

  {
    ASSERT_OK_AND_ASSIGN(int32_t client_pid,
                         RunPythonScript("src/stirling/mysql/testing/script.py"));

    // Sleep a little more, just to be safe.
    sleep(1);

    // Grab the data from Stirling.
    DataTable data_table(kMySQLTable);
    source_->TransferData(ctx_.get(), SocketTraceConnector::kMySQLTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    ASSERT_FALSE(tablets.empty());
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    // Check client-side tracing results.
    if (!FLAGS_tracing_mode) {
      std::vector<mysql::Record> records = GetTargetRecords(record_batch, client_pid);

      auto expected_records =
          mysql::JSONtoMySQLRecord("src/stirling/mysql/testing/mysql_container_bpf_test.json");

      std::vector<Matcher<mysql::Record>> expected_matchers;

      for (size_t i = 0; i < expected_records->size(); ++i) {
        expected_matchers.push_back(EqMySQLRecord((*expected_records)[i]));
      }

      EXPECT_THAT(records,
                  UnorderedElementsAreArray(expected_matchers.begin(), expected_matchers.end()));
    }
  }
}

}  // namespace stirling
}  // namespace pl
