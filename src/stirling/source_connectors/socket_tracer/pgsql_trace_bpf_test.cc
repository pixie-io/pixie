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

#include <absl/strings/str_split.h>

#include <string>
#include <string_view>
#include <utility>

#include "src/common/exec/exec.h"
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/golang_sqlx_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/postgresql_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using protocols::pgsql::Tag;
using ::px::stirling::testing::AccessRecordBatch;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;

class PostgreSQLTraceTest
    : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  PostgreSQLTraceTest() {
    PX_CHECK_OK(container_.Run(std::chrono::seconds{150}, {"--env=POSTGRES_PASSWORD=docker"}));
  }

  ::px::stirling::testing::PostgreSQLContainer container_;
};

struct ReqRespCmd {
  std::string req;
  std::string resp;
  std::string req_cmd;

  ReqRespCmd(std::string request, std::string response, std::string request_command)
      : req(std::move(request)), resp(std::move(response)), req_cmd(std::move(request_command)) {}
  ReqRespCmd(std::string request, std::string response, Tag tag)
      : ReqRespCmd(std::move(request), std::move(response), ToString(tag, /* is_req */ true)) {}
};

bool operator==(const ReqRespCmd& a, const ReqRespCmd& b) {
  return a.req == b.req && a.resp == b.resp && a.req_cmd == b.req_cmd;
}

std::ostream& operator<<(std::ostream& os, const ReqRespCmd& req_resp_cmd) {
  os << "(" << req_resp_cmd.req_cmd << ", " << req_resp_cmd.req << ", " << req_resp_cmd.resp << ")";
  return os;
}

std::vector<ReqRespCmd> RecordBatchToReqRespCmds(
    const types::ColumnWrapperRecordBatch& record_batch, const std::vector<size_t>& indices) {
  std::vector<ReqRespCmd> res;
  for (size_t i : indices) {
    res.emplace_back(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqIdx, i),
                     AccessRecordBatch<types::StringValue>(record_batch, kPGSQLRespIdx, i),
                     AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqCmdIdx, i));
  }
  return res;
}

// TODO(yzhao): We want to test Stirling's behavior when intercept the middle of the traffic.
// One way is to let SubProcess able to accept STDIN after launching. This test does not have that
// capability because it's running a query from start to finish, which always establish new
// connections.
TEST_F(PostgreSQLTraceTest, SelectQuery) {
  StartTransferDataThread();

  // --pid host is required to access the correct PID.
  constexpr char kCmdTmpl[] =
      "podman exec $0 bash -c "
      R"('psql -h localhost -U postgres -c "$1" &>/dev/null & echo $$! && wait')";
  const std::string kCreateTableCmd =
      absl::Substitute(kCmdTmpl, container_.container_name(),
                       "create table foo (field0 serial primary key);"
                       "insert into foo values (12345);"
                       "select * from foo;");
  ASSERT_OK_AND_ASSIGN(const std::string create_table_output, px::Exec(kCreateTableCmd));
  int32_t client_pid;
  ASSERT_TRUE(absl::SimpleAtoi(create_table_output, &client_pid));

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kPGSQLTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  auto indices = FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, client_pid);
  ASSERT_THAT(indices, SizeIs(1));

  EXPECT_THAT(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqIdx, indices[0]),
              HasSubstr("create table foo (field0 serial primary key);"
                        "insert into foo values (12345);"
                        "select * from foo;"));
  EXPECT_THAT(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLRespIdx, indices[0]),
              // TODO(PP-1920): This is a bug, it should return output for the other 2 queries.
              StrEq("CREATE TABLE"));
}

// Executes a demo golang app that queries PostgreSQL database with sqlx.
TEST_F(PostgreSQLTraceTest, GolangSqlxDemo) {
  // Uncomment to enable tracing:
  FLAGS_stirling_conn_trace_pid = container_.process_pid();

  StartTransferDataThread();

  ::px::stirling::testing::GolangSQLxContainer sqlx_container;
  PX_CHECK_OK(sqlx_container.Run(
      std::chrono::seconds{10},
      {absl::Substitute("--network=container:$0", container_.container_name())}));

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kPGSQLTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  // Select only the records from the client side. Stirling captures both client and server side
  // traffic because of the remote address is outside of the cluster.
  const auto indices =
      FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, sqlx_container.process_pid());

  EXPECT_THAT(
      RecordBatchToReqRespCmds(record_batch, indices),
      ElementsAre(
          ReqRespCmd(";", "", Tag::kQuery),
          ReqRespCmd("CREATE TABLE IF NOT EXISTS person (\n"
                     "    first_name text,\n"
                     "    last_name text,\n"
                     "    email text\n)",
                     "CREATE TABLE", Tag::kQuery),
          ReqRespCmd("BEGIN READ WRITE", "BEGIN", Tag::kQuery),
          ReqRespCmd("INSERT INTO person (first_name, last_name, email) VALUES ($1, $2, $3)",
                     "PARSE COMPLETE", Tag::kParse),
          ReqRespCmd("type=kStatement name=", "ROW DESCRIPTION []", Tag::kDesc),
          ReqRespCmd("portal= statement= parameters=[[format=kText value=Jason], "
                     "[format=kText value=Moiron], "
                     "[format=kText value=jmoiron@jmoiron.net]] result_format_codes=[]",
                     "BIND COMPLETE", Tag::kBind),
          ReqRespCmd("query=[INSERT INTO person (first_name, last_name, email) VALUES "
                     "($1, $2, $3)] params=[Jason, Moiron, jmoiron@jmoiron.net]",
                     "INSERT 0 1", Tag::kExecute),
          ReqRespCmd("COMMIT", "COMMIT", Tag::kQuery),
          ReqRespCmd("SELECT * FROM person WHERE first_name=$1", "PARSE COMPLETE", Tag::kParse),
          ReqRespCmd("type=kStatement name=",
                     "ROW DESCRIPTION [name=first_name table_oid=16384 attr_num=1 type_oid=25 "
                     "type_size=-1 type_modifier=-1 fmt_code=kText] "
                     "[name=last_name table_oid=16384 attr_num=2 type_oid=25 type_size=-1 "
                     "type_modifier=-1 fmt_code=kText] "
                     "[name=email table_oid=16384 attr_num=3 type_oid=25 type_size=-1 "
                     "type_modifier=-1 fmt_code=kText]",
                     Tag::kDesc),
          ReqRespCmd("portal= statement= parameters=[[format=kText value=Jason]] "
                     "result_format_codes=[]",
                     "BIND COMPLETE", Tag::kBind),
          ReqRespCmd("query=[SELECT * FROM person WHERE first_name=$1] params=[Jason]",
                     "Jason,Moiron,jmoiron@jmoiron.net\n"
                     "SELECT 1",
                     Tag::kExecute)));
}

TEST_F(PostgreSQLTraceTest, FunctionCall) {
  // --pid host is required to access the correct PID.
  constexpr char kCmdTmpl[] =
      "podman exec $0 bash -c "
      R"('psql -h localhost -U postgres -c "$1" &>/dev/null & echo $$! && wait')";
  {
    StartTransferDataThread();

    const std::string cmd = absl::Substitute(
        kCmdTmpl, container_.container_name(),
        "CREATE OR REPLACE FUNCTION increment(i integer) RETURNS integer AS \\$\\$\n"
        "BEGIN\n"
        "      RETURN i + 1;\n"
        "END;\n"
        "\\$\\$ LANGUAGE plpgsql;");
    ASSERT_OK_AND_ASSIGN(const std::string output, px::Exec(cmd));
    int32_t client_pid;
    ASSERT_TRUE(absl::SimpleAtoi(output, &client_pid));

    StopTransferDataThread();

    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kPGSQLTableNum);
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

    auto indices = FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, client_pid);
    ASSERT_THAT(indices, SizeIs(1));

    EXPECT_THAT(
        std::string(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqIdx, indices[0])),
        StrEq("CREATE OR REPLACE FUNCTION increment(i integer) RETURNS integer AS $$\n"
              "BEGIN\n"
              "      RETURN i + 1;\n"
              "END;\n"
              "$$ LANGUAGE plpgsql;"));
    EXPECT_THAT(
        std::string(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLRespIdx, indices[0])),
        StrEq("CREATE FUNCTION"));
    EXPECT_THAT(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqCmdIdx, indices[0]),
                StrEq(ToString(Tag::kQuery, /* is_req */ true)));
  }
  {
    StartTransferDataThread();

    const std::string cmd =
        absl::Substitute(kCmdTmpl, container_.container_name(), "select increment(1);");
    ASSERT_OK_AND_ASSIGN(const std::string output, px::Exec(cmd));
    int32_t client_pid;
    ASSERT_TRUE(absl::SimpleAtoi(output, &client_pid));

    StopTransferDataThread();

    std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kPGSQLTableNum);
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

    auto indices = FindRecordIdxMatchesPID(record_batch, kPGSQLUPIDIdx, client_pid);
    ASSERT_THAT(indices, SizeIs(1));

    EXPECT_THAT(
        std::string(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLReqIdx, indices[0])),
        StrEq("select increment(1);"));
    EXPECT_THAT(
        std::string(AccessRecordBatch<types::StringValue>(record_batch, kPGSQLRespIdx, indices[0])),
        StrEq("increment\n"
              "2\n"
              "SELECT 1"));
  }
}

}  // namespace stirling
}  // namespace px
