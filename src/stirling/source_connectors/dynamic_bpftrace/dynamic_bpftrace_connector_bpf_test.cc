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

#include <unistd.h>
#include <regex>

#include <gmock/gmock.h>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/testdata/cc/test_exe_fixture.h"
#include "src/stirling/source_connectors/dynamic_bpftrace/dynamic_bpftrace_connector.h"
#include "src/stirling/source_connectors/dynamic_bpftrace/utils.h"
#include "src/stirling/testing/common.h"

#include "src/stirling/proto/stirling.pb.h"

namespace px {
namespace stirling {

using ::google::protobuf::TextFormat;
using ::px::stirling::dynamic_tracing::ir::logical::TracepointDeployment;
using ::px::stirling::dynamic_tracing::ir::logical::TracepointDeployment_Tracepoint;
using ::px::stirling::dynamic_tracing::ir::shared::DeploymentSpec;
using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::RecordBatchSizeIs;
using ::px::stirling::testing::Timeout;
using ::px::testing::status::StatusIs;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::MatchesRegex;

// A regex for a string of printable characters. See ASCII table.
constexpr char kPrintableRegex[] = "[ -~]*";

TEST(DynamicBPFTraceConnectorTest, Basic) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
    printf(" aaa time_:%llu pid:%u value:%llu aaa command:%s address:%s aaa\n", nsecs, pid, 0, comm, ntop(0));
  })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SourceConnector> connector,
                       DynamicBPFTraceConnector::Create("test", tracepoint));

  const int kTableNum = 0;
  const DataTableSchema& table_schema = connector->table_schemas()[kTableNum];

  // Check the inferred table schema.
  {
    const ArrayView<DataElement>& elements = table_schema.elements();

    ASSERT_EQ(elements.size(), 5);

    EXPECT_EQ(elements[0].name(), "time_");
    EXPECT_EQ(elements[0].type(), types::DataType::TIME64NS);

    EXPECT_EQ(elements[1].name(), "pid");
    EXPECT_EQ(elements[1].type(), types::DataType::INT64);

    EXPECT_EQ(elements[2].name(), "value");
    EXPECT_EQ(elements[2].type(), types::DataType::INT64);

    EXPECT_EQ(elements[3].name(), "command");
    EXPECT_EQ(elements[3].type(), types::DataType::STRING);

    EXPECT_EQ(elements[4].name(), "address");
    EXPECT_EQ(elements[4].type(), types::DataType::STRING);
  }

  // Now deploy the spec and check for some data.
  ASSERT_OK(connector->Init());

  // Wait for data to be collected.
  std::vector<TaggedRecordBatch> tablets;
  Timeout t;
  while (tablets.size() == 0 && !t.TimedOut()) {
    // Read the data.
    SystemWideStandaloneContext ctx;
    DataTable data_table(/*id*/ 0, table_schema);
    connector->set_data_tables({&data_table});
    connector->TransferData(&ctx);
    tablets = data_table.ConsumeRecords();
  }

  // Should've gotten something in the records.
  ASSERT_FALSE(tablets.empty());

  // Check that we can gracefully wrap-up.
  ASSERT_OK(connector->Stop());
}

TEST(DynamicBPFTraceConnectorTest, BPFTraceBuiltins) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
     printf("pid:%llu tid:%llu uid:%llu gid:%llu nsecs:%llu elapsed:%llu cpu:%llu comm:%s kstack:%s",
            pid, tid, uid, gid, nsecs, elapsed, cpu, comm, kstack);
  })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SourceConnector> connector,
                       DynamicBPFTraceConnector::Create("test", tracepoint));

  const int kTableNum = 0;
  const DataTableSchema& table_schema = connector->table_schemas()[kTableNum];

  const int kPIDIdx = 0;
  const int kTIDIdx = 1;
  const int kUIDIdx = 2;
  const int kGIDIdx = 3;
  const int kNsecsIdx = 4;
  const int kElapsedIdx = 5;
  const int kCPUIdx = 6;
  const int kCommIdx = 7;
  const int kStackIdx = 8;

  // Check the inferred table schema.
  {
    const ArrayView<DataElement>& elements = table_schema.elements();

    ASSERT_EQ(elements.size(), 9);

    EXPECT_EQ(elements[kPIDIdx].name(), "pid");
    EXPECT_EQ(elements[kPIDIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kTIDIdx].name(), "tid");
    EXPECT_EQ(elements[kTIDIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kUIDIdx].name(), "uid");
    EXPECT_EQ(elements[kUIDIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kGIDIdx].name(), "gid");
    EXPECT_EQ(elements[kGIDIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kNsecsIdx].name(), "nsecs");
    EXPECT_EQ(elements[kNsecsIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kElapsedIdx].name(), "elapsed");
    EXPECT_EQ(elements[kElapsedIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kCPUIdx].name(), "cpu");
    EXPECT_EQ(elements[kCPUIdx].type(), types::DataType::INT64);

    EXPECT_EQ(elements[kCommIdx].name(), "comm");
    EXPECT_EQ(elements[kCommIdx].type(), types::DataType::STRING);

    EXPECT_EQ(elements[kStackIdx].name(), "kstack");
    EXPECT_EQ(elements[kStackIdx].type(), types::DataType::STRING);
  }

  auto deploy_start = px::chrono::boot_clock::now().time_since_epoch();
  auto deploy_start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(deploy_start).count();
  // Now deploy the spec and check for some data.
  ASSERT_OK(connector->Init());

  // Wait for data to be collected.
  std::vector<TaggedRecordBatch> tablets;
  Timeout t;
  while (tablets.size() == 0 && !t.TimedOut()) {
    // Read the data.
    SystemWideStandaloneContext ctx;
    DataTable data_table(/*id*/ 0, table_schema);
    connector->set_data_tables({&data_table});
    connector->TransferData(&ctx);
    tablets = data_table.ConsumeRecords();
  }

  // Should've gotten something in the records.
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& records, tablets);

  // TODO(oazizi): Use /proc/sys/kernel/pid_max to make more robust.
  const uint64_t kMaxPIDValue = 1ULL << 22;
  const uint64_t kMaxUIDValue = 1ULL << 32;

  // Check the first record for reasonable values.
  {
    int64_t pid = records[kPIDIdx]->Get<types::Int64Value>(0).val;
    LOG(INFO) << absl::Substitute("PID: $0", pid);
    EXPECT_GE(pid, 0);
    EXPECT_LE(pid, kMaxPIDValue);

    int64_t tid = records[kTIDIdx]->Get<types::Int64Value>(0).val;
    LOG(INFO) << absl::Substitute("TID: $0", tid);
    EXPECT_GE(tid, 0);
    EXPECT_LE(tid, kMaxPIDValue);

    int64_t uid = records[kUIDIdx]->Get<types::Int64Value>(0).val;
    LOG(INFO) << absl::Substitute("UID: $0", uid);
    EXPECT_GE(uid, 0);
    EXPECT_LE(uid, kMaxUIDValue);

    int64_t gid = records[kGIDIdx]->Get<types::Int64Value>(0).val;
    LOG(INFO) << absl::Substitute("GID: $0", gid);
    EXPECT_GE(gid, 0);
    EXPECT_LE(gid, kMaxUIDValue);

    int64_t nsecs = records[kNsecsIdx]->Get<types::Int64Value>(0).val;
    LOG(INFO) << absl::Substitute("nsecs: $0", nsecs);
    // Check that the timestamp is after the connector was initialized, and before the current
    // timestamp.
    auto now = px::chrono::boot_clock::now().time_since_epoch();
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    EXPECT_GE(nsecs, deploy_start_ns);
    EXPECT_LE(nsecs, now_ns);

    int64_t cpu = records[kCPUIdx]->Get<types::Int64Value>(0).val;
    LOG(INFO) << absl::Substitute("CPU: $0", cpu);
    EXPECT_GE(cpu, 0);
    EXPECT_LE(cpu, 100);

    // comm
    std::string comm = records[kCommIdx]->Get<types::StringValue>(0);
    LOG(INFO) << absl::Substitute("comm: $0", comm);
    EXPECT_THAT(comm, MatchesRegex(kPrintableRegex));
  }

  // Check that we can gracefully wrap-up.
  ASSERT_OK(connector->Stop());
}

TEST(DynamicBPFTraceConnectorTest, BPFTraceBuiltins2) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
       printf("username:%s ftime:%s inet:%s",
               username, strftime("%H:%M:%S", nsecs), ntop(0));
    })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SourceConnector> connector,
                       DynamicBPFTraceConnector::Create("test", tracepoint));

  const int kTableNum = 0;
  const DataTableSchema& table_schema = connector->table_schemas()[kTableNum];

  const int kUsernameIdx = 0;
  const int kFTimeIdx = 1;
  const int kInetIdx = 2;

  // Check the inferred table schema.
  {
    const ArrayView<DataElement>& elements = table_schema.elements();

    ASSERT_EQ(elements.size(), 3);

    EXPECT_EQ(elements[kUsernameIdx].name(), "username");
    EXPECT_EQ(elements[kUsernameIdx].type(), types::DataType::STRING);

    EXPECT_EQ(elements[kFTimeIdx].name(), "ftime");
    EXPECT_EQ(elements[kFTimeIdx].type(), types::DataType::STRING);

    EXPECT_EQ(elements[kInetIdx].name(), "inet");
    EXPECT_EQ(elements[kInetIdx].type(), types::DataType::STRING);
  }

  // Now deploy the spec and check for some data.
  ASSERT_OK(connector->Init());

  // Wait for data to be collected.
  std::vector<TaggedRecordBatch> tablets;
  Timeout t;
  while (tablets.size() == 0 && !t.TimedOut()) {
    // Read the data.
    SystemWideStandaloneContext ctx;
    DataTable data_table(/*id*/ 0, table_schema);
    connector->set_data_tables({&data_table});
    connector->TransferData(&ctx);
    tablets = data_table.ConsumeRecords();
  }

  // Should've gotten something in the records.
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& records, tablets);

  std::string username = records[kUsernameIdx]->Get<types::StringValue>(0);
  LOG(INFO) << absl::Substitute("username: $0", username);
  EXPECT_THAT(username, MatchesRegex(kPrintableRegex));

  std::string ftime = records[kFTimeIdx]->Get<types::StringValue>(0);
  LOG(INFO) << absl::Substitute("ftime: $0", ftime);
  EXPECT_THAT(ftime, MatchesRegex("[0-2][0-9]:[0-5][0-9]:[0-5][0-9]"));

  std::string inet = records[kInetIdx]->Get<types::StringValue>(0);
  LOG(INFO) << absl::Substitute("inet: $0", inet);
  EXPECT_EQ(inet, "0.0.0.0");

  // Check that we can gracefully wrap-up.
  ASSERT_OK(connector->Stop());
}

TEST(DynamicBPFTraceConnectorTest, BPFTraceUnlabeledColumn) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
       printf("username:%s foo   %s inet:%s",
               username, strftime("%H:%M:%S", nsecs), ntop(0));
    })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SourceConnector> connector,
                       DynamicBPFTraceConnector::Create("test", tracepoint));

  const int kTableNum = 0;
  const DataTableSchema& table_schema = connector->table_schemas()[kTableNum];

  const int kUsernameIdx = 0;
  const int kFTimeIdx = 1;
  const int kInetIdx = 2;

  // Check the inferred table schema.
  {
    const ArrayView<DataElement>& elements = table_schema.elements();

    ASSERT_EQ(elements.size(), 3);

    EXPECT_EQ(elements[kUsernameIdx].name(), "username");
    EXPECT_EQ(elements[kUsernameIdx].type(), types::DataType::STRING);

    EXPECT_EQ(elements[kFTimeIdx].name(), "Column_1");
    EXPECT_EQ(elements[kFTimeIdx].type(), types::DataType::STRING);

    EXPECT_EQ(elements[kInetIdx].name(), "inet");
    EXPECT_EQ(elements[kInetIdx].type(), types::DataType::STRING);
  }

  // Now deploy the spec and check for some data.
  ASSERT_OK(connector->Init());

  // Wait for data to be collected.
  std::vector<TaggedRecordBatch> tablets;
  Timeout t;
  while (tablets.size() == 0 && !t.TimedOut()) {
    // Read the data.
    SystemWideStandaloneContext ctx;
    DataTable data_table(/*id*/ 0, table_schema);
    connector->set_data_tables({&data_table});
    connector->TransferData(&ctx);
    tablets = data_table.ConsumeRecords();
  }

  // Should've gotten something in the records.
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& records, tablets);

  std::string username = records[kUsernameIdx]->Get<types::StringValue>(0);
  LOG(INFO) << absl::Substitute("username: $0", username);
  EXPECT_THAT(username, MatchesRegex(kPrintableRegex));

  std::string ftime = records[kFTimeIdx]->Get<types::StringValue>(0);
  LOG(INFO) << absl::Substitute("ftime: $0", ftime);
  EXPECT_THAT(ftime, MatchesRegex("[0-2][0-9]:[0-5][0-9]:[0-5][0-9]"));

  std::string inet = records[kInetIdx]->Get<types::StringValue>(0);
  LOG(INFO) << absl::Substitute("inet: $0", inet);
  EXPECT_EQ(inet, "0.0.0.0");

  // Check that we can gracefully wrap-up.
  ASSERT_OK(connector->Stop());
}

TEST(DynamicBPFTraceConnectorTest, BPFTraceSyntacticError) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
           bogus(;
           printf("username:%s time:%s", username, nsecs);
        })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  // TODO(oazizi): Find a way to get the clang error passed up.
  ASSERT_THAT(
      DynamicBPFTraceConnector::Create("test", tracepoint).status(),
      StatusIs(statuspb::INTERNAL, HasSubstr("Could not compile bpftrace script, failed to parse: "
                                             "stdin:2:12-19: ERROR: syntax error, unexpected ;\n"
                                             "           bogus(;")));
}

TEST(DynamicBPFTraceConnectorTest, BPFTraceTracepointFormatError) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(tracepoint:test:test {
           printf("username:%d time:%d", nsecs, nsecs);
        })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  // TODO(oazizi): Find a way to get the clang error passed up.
  ASSERT_THAT(DynamicBPFTraceConnector::Create("test", tracepoint).status(),
              StatusIs(statuspb::INTERNAL,
                       HasSubstr("Could not compile bpftrace script, invalid tracepoint: "
                                 "stdin:1:1-21: ERROR: tracepoint not found: test:test")));
}

TEST(DynamicBPFTraceConnectorTest, BPFTraceSemanticError) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
         printf("username:%s foo   %s inet:%s",
                 username, strftime("%H:%M:%S", nsecs), ntop(0), 0);
      })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  ASSERT_THAT(DynamicBPFTraceConnector::Create("test", tracepoint).status(),
              StatusIs(statuspb::INTERNAL,
                       HasSubstr("ERROR: printf: Too many arguments for format string")));
}

// TODO(yzhao): Add a test to check the error message thrown by ClangParser::parse().
// See https://github.com/iovisor/bpftrace/discussions/2210 for possible suggestions on how to
// trigger the error.

TEST(DynamicBPFTraceConnectorTest, BPFTraceCheckPrintfsError) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
           printf("time_:%llu val:%d inet:%s", nsecs, 1, "true");
           printf("time_:%llu name:%s", nsecs, "hello");
        })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  ASSERT_THAT(
      DynamicBPFTraceConnector::Create("test", tracepoint).status(),
      StatusIs(statuspb::INTERNAL,
               HasSubstr("All printf statements must have exactly the same format string")));
}

constexpr std::string_view kServerPath_1_16 =
    "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/"
    "golang_1_16_grpc_server";
constexpr std::string_view kServerPath_1_17 =
    "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/"
    "golang_1_17_grpc_server";

TEST(DynamicBPFTraceConnectorTest, InsertUProbeTargetObjPaths) {
  std::string go1_16_binary_path = px::testing::BazelRunfilePath(kServerPath_1_16).string();
  std::string go1_17_binary_path = px::testing::BazelRunfilePath(kServerPath_1_17).string();

  ASSERT_TRUE(fs::Exists(go1_16_binary_path));
  ASSERT_TRUE(fs::Exists(go1_17_binary_path));

  DeploymentSpec spec;
  spec.mutable_path_list()->add_paths(go1_16_binary_path);
  spec.mutable_path_list()->add_paths(go1_17_binary_path);

  std::string uprobe_script =
      "// Deploys uprobes to trace http2 traffic.\n"
      "uprobe:\"golang.org/x/net/http2.(*Framer).WriteDataPadded\""
      "{ printf(\"stream_id: %d, end_stream: %d\", arg0, arg1); }\n"
      "uretprobe:\"golang.org/x/net/http2.(*Framer).WriteDataPadded\""
      "{ printf(\"retval: %d\", retval); }";
  InsertUprobeTargetObjPaths(spec, &uprobe_script);
  EXPECT_EQ(
      uprobe_script,
      absl::StrCat("// Deploys uprobes to trace http2 traffic.\n", "uprobe:", go1_16_binary_path,
                   ":\"golang.org/x/net/http2.(*Framer).WriteDataPadded\",\n"
                   "uprobe:",
                   go1_17_binary_path,
                   ":\"golang.org/x/net/http2.(*Framer).WriteDataPadded\""
                   "{ printf(\"stream_id: %d, end_stream: %d\", arg0, arg1); }\n",
                   "uretprobe:", go1_16_binary_path,
                   ":\"golang.org/x/net/http2.(*Framer).WriteDataPadded\",\n"
                   "uretprobe:",
                   go1_17_binary_path,
                   ":\"golang.org/x/net/http2.(*Framer).WriteDataPadded\""
                   "{ printf(\"retval: %d\", retval); }"));
}

class CPPDynamicBPFTraceTest : public ::testing::Test {
 protected:
  void InitTestFixturesAndRunTestProgram(const std::string& text_pb) {
    CHECK(TextFormat::ParseFromString(text_pb, &deployment_));

    ASSERT_TRUE(fs::Exists(test_exe_fixture_.Path()));
    deployment_.mutable_deployment_spec()->mutable_path_list()->add_paths(
        test_exe_fixture_.Path().string());

    auto tracepoint = deployment_.tracepoints(0);
    std::string* script = tracepoint.mutable_bpftrace()->mutable_program();
    if (ContainsUProbe(*script)) {
      InsertUprobeTargetObjPaths(deployment_.deployment_spec(), script);
    }

    ASSERT_OK_AND_ASSIGN(connector_,
                         DynamicBPFTraceConnector::Create("my_dynamic_source", tracepoint));

    ASSERT_OK(connector_->Init());

    ASSERT_OK(test_exe_fixture_.Run());
  }

  std::vector<TaggedRecordBatch> GetRecords() {
    constexpr int kTableNum = 0;
    auto ctx = std::make_unique<SystemWideStandaloneContext>();
    auto data_table = std::make_unique<DataTable>(/*id*/ 0, connector_->table_schemas()[kTableNum]);
    connector_->set_data_tables({data_table.get()});
    connector_->TransferData(ctx.get());
    return data_table->ConsumeRecords();
  }

  // Need debug build to include the dwarf info.
  obj_tools::TestExeFixture test_exe_fixture_;

  TracepointDeployment deployment_;
  std::unique_ptr<SourceConnector> connector_;
};

constexpr char kTestExeTraceProgram[] = R"(
tracepoints {
  bpftrace {
    program: "uprobe:\"px::testing::Foo::Bar\"{ printf(\"ptr: %d, i: %d\", arg0, arg1); }"
  }
}
)";

TEST_F(CPPDynamicBPFTraceTest, TraceTestExe) {
  ASSERT_NO_FATAL_FAILURE(InitTestFixturesAndRunTestProgram(kTestExeTraceProgram));
  std::vector<TaggedRecordBatch> tablets = GetRecords();

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ASSERT_THAT(record_batch, RecordBatchSizeIs(10));

  constexpr size_t kArgIdx = 1;

  EXPECT_EQ(record_batch[kArgIdx]->Get<types::Int64Value>(0).val, 3);
}

}  // namespace stirling
}  // namespace px
