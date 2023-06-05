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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include <absl/strings/str_replace.h>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/test_output_generator/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/mysql_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/python_mysql_connector_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace mysql = protocols::mysql;

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::testing::BazelRunfilePath;
using ::px::types::ColumnWrapper;
using ::px::types::ColumnWrapperRecordBatch;

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

class MySQLTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  MySQLTraceTest() {
    std::string script_path = BazelRunfilePath(
        "src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/script.sql");
    LOG(INFO) << script_path;

    // Run the MySQL server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    StatusOr<std::string> run_result =
        server_.Run(std::chrono::seconds{90},
                    {"--env=MYSQL_ALLOW_EMPTY_PASSWORD=1", "--env=MYSQL_ROOT_HOST=%"});
    PX_CHECK_OK(run_result);

    // Sleep an additional second, just to be safe.
    sleep(1);
  }

  StatusOr<int32_t> RunSQLScript(std::string_view script_path) {
    std::string absl_script_path = BazelRunfilePath(script_path);
    PX_ASSIGN_OR_RETURN(std::string script_content, px::ReadFileToString(absl_script_path));

    // Since script content will be passed through bash, escape any single quotes in the script.
    script_content = absl::StrReplaceAll(script_content, {{"'", "'\\''"}});

    // Run mysql as a way of generating traffic.
    // Run it through bash, and return the PID, so we can use it to filter captured results.
    std::string cmd = absl::StrFormat(
        "podman exec %s bash -c 'echo \"%s\" | mysql --protocol=TCP --ssl-mode=DISABLED "
        "--host=localhost --port=3306 -uroot & echo $! && wait'",
        server_.container_name(), script_content);
    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));

    std::vector<std::string_view> lines = absl::StrSplit(out, "\n");
    if (lines.empty()) {
      return error::Internal("Executed output (pid) from command.");
    }

    int32_t client_pid;
    if (!absl::SimpleAtoi(lines[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    LOG(INFO) << absl::Substitute("Client PID: $0", client_pid);

    return client_pid;
  }

  StatusOr<int32_t> RunPythonScript(std::string_view script_path) {
    std::filesystem::path script_file_path = BazelRunfilePath(script_path);
    PX_ASSIGN_OR_RETURN(script_file_path, fs::Canonical(script_file_path));
    std::filesystem::path script_dir = script_file_path.parent_path();
    std::filesystem::path script_filename = script_file_path.filename();

    PX_ASSIGN_OR_RETURN(
        std::string out,
        client_.Run(std::chrono::seconds{60},
                    {absl::Substitute("--network=container:$0", server_.container_name())},
                    {"/scripts/" + script_filename.string()}));
    LOG(INFO) << "Script output\n" << out;
    client_.Wait();

    // The first line of the output should be pid=<pid> (the container's python init is set-up to
    // print this automatically). Parse out the client PID from this line.
    std::vector<std::string_view> lines = absl::StrSplit(out, "\n");
    if (lines.empty()) {
      return error::Internal("Expected output from command.");
    }

    std::vector<std::string_view> tokens = absl::StrSplit(lines[0], "=");
    if (tokens.size() != 2) {
      return error::Internal("Expected first line to be of format: pid=<pid>");
    }

    int32_t client_pid;
    if (!absl::SimpleAtoi(tokens[1], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    LOG(INFO) << absl::Substitute("Client PID: $0", client_pid);

    return client_pid;
  }

  ::px::stirling::testing::MySQLContainer server_;
  ::px::stirling::testing::PythonMySQLConnectorContainer client_;
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
    StartTransferDataThread();

    ASSERT_OK_AND_ASSIGN(
        int32_t client_pid,
        RunSQLScript(
            "src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/script.sql"));

    StopTransferDataThread();

    // Grab the data from Stirling.
    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kMySQLTableNum);
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

    if (!FLAGS_tracing_mode) {
      // Check client-side tracing results.
      std::vector<mysql::Record> client_records = GetTargetRecords(record_batch, client_pid);

      EXPECT_THAT(
          client_records,
          UnorderedElementsAre(EqMySQLRecord(kRecordInit), EqMySQLRecord(kRecordScript1Cmd1),
                               EqMySQLRecord(kRecordScript1Cmd2), EqMySQLRecord(kRecordScript1Cmd3),
                               EqMySQLRecord(kRecordScript1Cmd4), EqMySQLRecord(kRecordScript1Cmd5),
                               EqMySQLRecord(kRecordScript1Cmd6)));

      // Check server-side tracing results.
      std::vector<mysql::Record> server_records =
          GetTargetRecords(record_batch, server_.process_pid());

      // Here, the Init Record is omitted on purpose, because MySQL server reads in 4-byte header
      // and the rest of the packet in 2 different reads. To avoid complicating BPF code, we choose
      // to loss the first packet on the server side, which is used for protocol inference.
      EXPECT_THAT(server_records,
                  UnorderedElementsAre(
                      EqMySQLRecord(kRecordScript1Cmd1), EqMySQLRecord(kRecordScript1Cmd2),
                      EqMySQLRecord(kRecordScript1Cmd3), EqMySQLRecord(kRecordScript1Cmd4),
                      EqMySQLRecord(kRecordScript1Cmd5), EqMySQLRecord(kRecordScript1Cmd6)));
    }
  }

  {
    StartTransferDataThread();

    ASSERT_OK_AND_ASSIGN(int32_t client_pid,
                         RunSQLScript("src/stirling/source_connectors/socket_tracer/protocols/"
                                      "mysql/testing/prepare_execute.sql"));

    StopTransferDataThread();

    // Grab the data from Stirling.
    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kMySQLTableNum);
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

    if (!FLAGS_tracing_mode) {
      // Check client-side tracing results.
      std::vector<mysql::Record> client_records = GetTargetRecords(record_batch, client_pid);

      EXPECT_THAT(
          client_records,
          UnorderedElementsAre(EqMySQLRecord(kRecordInit), EqMySQLRecord(kRecordScript2Cmd1),
                               EqMySQLRecord(kRecordScript2Cmd2), EqMySQLRecord(kRecordScript2Cmd3),
                               EqMySQLRecord(kRecordScript2Cmd4), EqMySQLRecord(kRecordScript2Cmd5),
                               EqMySQLRecord(kRecordScript2Cmd6), EqMySQLRecord(kRecordScript2Cmd7),
                               EqMySQLRecord(kRecordScript2Cmd8)));

      // Check server-side tracing results.
      std::vector<mysql::Record> server_records =
          GetTargetRecords(record_batch, server_.process_pid());

      EXPECT_THAT(server_records,
                  UnorderedElementsAre(
                      EqMySQLRecord(kRecordScript2Cmd1), EqMySQLRecord(kRecordScript2Cmd2),
                      EqMySQLRecord(kRecordScript2Cmd3), EqMySQLRecord(kRecordScript2Cmd4),
                      EqMySQLRecord(kRecordScript2Cmd5), EqMySQLRecord(kRecordScript2Cmd6),
                      EqMySQLRecord(kRecordScript2Cmd7), EqMySQLRecord(kRecordScript2Cmd8)));
    }
  }

  {
    StartTransferDataThread();

    ASSERT_OK_AND_ASSIGN(
        int32_t client_pid,
        RunPythonScript(
            "src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/script.py"));

    // Sleep a little more, just to be safe.
    sleep(1);

    StopTransferDataThread();

    // Grab the data from Stirling.
    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kMySQLTableNum);
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

    // Check client-side tracing results.
    if (!FLAGS_tracing_mode) {
      std::vector<mysql::Record> records = GetTargetRecords(record_batch, client_pid);

      auto expected_records = mysql::JSONtoMySQLRecord(
          "src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/"
          "mysql_container_bpf_test.json");

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
}  // namespace px
