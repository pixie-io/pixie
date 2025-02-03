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
      name: "can_you_find_this_output"
      fields: "a"
      fields: "b"
    }
    probes: {
      name: "probe_foo_bar"
      tracepoint: {
        symbol: "CanYouFindThis"
        type: LOGICAL
      }
      args {
        id: "can_you_find_this_arg_a"
        expr: "a"
      }
      args {
        id: "can_you_find_this_arg_b"
        expr: "b"
      }
      output_actions {
        output_name: "can_you_find_this_output"
        variable_names: "can_you_find_this_arg_a"
        variable_names: "can_you_find_this_arg_b"
      }
    }
  }
}
)";

TEST_F(CPPDynamicTraceTest, TraceTestExe) {
  ASSERT_NO_FATAL_FAILURE(InitTestFixturesAndRunTestProgram(kTestExeTraceProgram));
  // The DataTable has the following indices:
  // 0: UPID
  // 1: time_
  // 2: can_you_find_this_output.a
  // 3: can_you_find_this_output.b
  auto a_field_idx = 2;
  auto b_field_idx = 3;
  LOG(INFO) << a_field_idx;
  std::vector<TaggedRecordBatch> tablets = GetRecords();
  for (const auto& tablet : tablets) {
    // TestExeFixture calls CanYouFindThis(3, 4) 10 times
    EXPECT_EQ(tablet.records[a_field_idx]->Size(), 10);
    EXPECT_EQ(tablet.records[b_field_idx]->Size(), 10);
    EXPECT_EQ(tablet.records[a_field_idx]->Get<types::Int64Value>(0).val, 3);
    EXPECT_EQ(tablet.records[b_field_idx]->Get<types::Int64Value>(0).val, 4);
  }
}

}  // namespace stirling
}  // namespace px
