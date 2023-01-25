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

#include <absl/strings/substitute.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/types.h"
#include "src/stirling/obj_tools/testdata/cc/test_exe_fixture.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_trace_connector.h"
#include "src/stirling/testing/common.h"

#include "src/stirling/proto/stirling.pb.h"

constexpr std::string_view kClientPath =
    "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_client/"
    "golang_1_16_grpc_client";
constexpr std::string_view kServerPath =
    "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/"
    "golang_1_16_grpc_server";

namespace px {
namespace stirling {

using ::google::protobuf::TextFormat;

using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::RecordBatchSizeIs;
using ::testing::Each;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::SizeIs;
using ::testing::StrEq;

using LogicalProgram = ::px::stirling::dynamic_tracing::ir::logical::TracepointDeployment;

// TODO(yzhao): Create test fixture that wraps the test binaries.
class GoHTTPDynamicTraceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    client_path_ = px::testing::BazelRunfilePath(kClientPath).string();
    server_path_ = px::testing::BazelRunfilePath(kServerPath).string();

    ASSERT_TRUE(fs::Exists(server_path_));
    ASSERT_TRUE(fs::Exists(client_path_));

    ASSERT_OK(s_.Start({server_path_, "--port=0"}));

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    ASSERT_OK(s_.Stdout(&port_str));
    ASSERT_TRUE(absl::SimpleAtoi(port_str, &s_port_));
    ASSERT_NE(0, s_port_);
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
  }

  void InitTestFixturesAndRunTestProgram(const std::string& text_pb) {
    CHECK(TextFormat::ParseFromString(text_pb, &logical_program_));

    logical_program_.mutable_deployment_spec()->mutable_path_list()->add_paths(server_path_);

    ASSERT_OK_AND_ASSIGN(connector_,
                         DynamicTraceConnector::Create("my_dynamic_source", &logical_program_));
    ASSERT_OK(connector_->Init());

    ASSERT_OK(c_.Start({client_path_, "-name=PixieLabs", "-count=10",
                        absl::StrCat("-address=localhost:", s_port_)}));
    EXPECT_EQ(0, c_.Wait()) << "Client should be killed";
  }

  std::vector<TaggedRecordBatch> GetRecords() {
    constexpr int kTableNum = 0;
    auto ctx = std::make_unique<SystemWideStandaloneContext>();
    auto data_table = std::make_unique<DataTable>(/*id*/ 0, connector_->table_schemas()[kTableNum]);
    connector_->set_data_tables({data_table.get()});
    connector_->TransferData(ctx.get());
    return data_table->ConsumeRecords();
  }

  std::string server_path_;
  std::string client_path_;

  SubProcess c_;
  SubProcess s_;
  int s_port_ = 0;

  LogicalProgram logical_program_;
  std::unique_ptr<SourceConnector> connector_;
};

constexpr char kGRPCTraceProgram[] = R"(
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "probe_WriteDataPadded_table"
      fields: "stream_id"
      fields: "end_stream"
      fields: "latency"
    }
    probes: {
      name: "probe_WriteDataPadded"
      tracepoint: {
        symbol: "golang.org/x/net/http2.(*Framer).WriteDataPadded"
        type: LOGICAL
      }
      args {
        id: "stream_id"
        expr: "streamID"
      }
      args {
        id: "end_stream"
        expr: "endStream"
      }
      function_latency { id: "latency" }
      output_actions {
        output_name: "probe_WriteDataPadded_table"
        variable_names: "stream_id"
        variable_names: "end_stream"
        variable_names: "latency"
      }
    }
  }
}
)";

constexpr char kReturnValueTraceProgram[] = R"(
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "probe_readFrameHeader"
      fields: "frame_header_valid"
    }
    probes: {
      name: "probe_StreamEnded"
      tracepoint: {
        symbol: "golang.org/x/net/http2.readFrameHeader"
        type: LOGICAL
      }
      ret_vals {
        id: "frame_header_valid"
        expr: "$0.valid"
      }
      output_actions {
        output_name: "probe_readFrameHeader"
        variable_names: "frame_header_valid"
      }
    }
  }
}
)";

TEST_F(GoHTTPDynamicTraceTest, TraceGolangHTTPClientAndServer) {
  ASSERT_NO_FATAL_FAILURE(InitTestFixturesAndRunTestProgram(kGRPCTraceProgram));
  std::vector<TaggedRecordBatch> tablets = GetRecords();

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, /*index*/ 0, s_.child_pid());

    ASSERT_THAT(records, RecordBatchSizeIs(10));

    constexpr size_t kStreamIDIdx = 3;
    constexpr size_t kEndStreamIdx = 4;
    constexpr size_t kLatencyIdx = 5;

    EXPECT_EQ(records[kStreamIDIdx]->Get<types::Int64Value>(0).val, 1);
    EXPECT_EQ(records[kEndStreamIdx]->Get<types::BoolValue>(0).val, false);
    // 1000 is not particularly meaningful, it just states that we have a roughly correct
    // value.
    EXPECT_THAT(records[kLatencyIdx]->Get<types::Int64Value>(0).val, Gt(1000));
  }
}

TEST_F(GoHTTPDynamicTraceTest, TraceReturnValue) {
  ASSERT_NO_FATAL_FAILURE(InitTestFixturesAndRunTestProgram(kReturnValueTraceProgram));
  std::vector<TaggedRecordBatch> tablets = GetRecords();

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, /*index*/ 0, s_.child_pid());

    ASSERT_THAT(records, RecordBatchSizeIs(80));

    constexpr size_t kFrameHeaderValidIdx = 3;

    EXPECT_EQ(records[kFrameHeaderValidIdx]->Get<types::BoolValue>(0).val, true);
  }
}

class CPPDynamicTraceTest : public ::testing::Test {
 protected:
  void InitTestFixturesAndRunTestProgram(const std::string& text_pb) {
    CHECK(TextFormat::ParseFromString(text_pb, &logical_program_));

    logical_program_.mutable_deployment_spec()->mutable_path_list()->add_paths(
        test_exe_fixture_.Path());

    ASSERT_OK_AND_ASSIGN(connector_,
                         DynamicTraceConnector::Create("my_dynamic_source", &logical_program_));

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

  LogicalProgram logical_program_;
  std::unique_ptr<SourceConnector> connector_;
};

constexpr char kTestExeTraceProgram[] = R"(
tracepoints {
  program {
    language: CPP
    outputs {
      name: "foo_bar_output"
      fields: "arg"
    }
    probes: {
      name: "probe_foo_bar"
      tracepoint: {
        symbol: "px::testing::Foo::Bar"
        type: LOGICAL
      }
      args {
        id: "foo_bar_arg"
        expr: "i"
      }
      output_actions {
        output_name: "foo_bar_output"
        variable_names: "foo_bar_arg"
      }
    }
  }
}
)";

TEST_F(CPPDynamicTraceTest, DISABLED_TraceTestExe) {
  // TODO(yzhao): This does not work yet.
  ASSERT_NO_FATAL_FAILURE(InitTestFixturesAndRunTestProgram(kTestExeTraceProgram));
  std::vector<TaggedRecordBatch> tablets = GetRecords();
  PX_UNUSED(tablets);
}

}  // namespace stirling
}  // namespace px
