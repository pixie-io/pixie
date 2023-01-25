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

#include <rapidjson/document.h>
#include <functional>
#include <thread>
#include <utility>

#include <absl/functional/bind_front.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/stirling.h"

namespace px {
namespace stirling {

using ::px::testing::BazelRunfilePath;
using ::testing::SizeIs;
using ::testing::StrEq;

//-----------------------------------------------------------------------------
// Test fixture and shared code
//-----------------------------------------------------------------------------

// Utility to run a binary as a trace target.
// Performs automatic clean-up.
class BinaryRunner {
 public:
  void Run(const std::string& binary_path) {
    // Run tracing target.
    ASSERT_TRUE(fs::Exists(binary_path));
    ASSERT_OK(trace_target_.Start({binary_path}));
  }

  ~BinaryRunner() { trace_target_.Kill(); }

 private:
  SubProcess trace_target_;
};

class StirlingDynamicTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    stirling_ = Stirling::Create(std::move(registry));

    // Set function to call on data pushes.
    stirling_->RegisterDataPushCallback(
        absl::bind_front(&StirlingDynamicTraceBPFTest::AppendData, this));
  }

  Status AppendData(uint64_t table_id, types::TabletID tablet_id,
                    std::unique_ptr<types::ColumnWrapperRecordBatch> record_batch) {
    PX_UNUSED(table_id);
    PX_UNUSED(tablet_id);
    record_batches_.push_back(std::move(record_batch));
    return Status::OK();
  }

  StatusOr<stirlingpb::Publish> WaitForStatus(sole::uuid trace_id) {
    StatusOr<stirlingpb::Publish> s;
    do {
      s = stirling_->GetTracepointInfo(trace_id);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (!s.ok() && s.code() == px::statuspb::Code::RESOURCE_UNAVAILABLE);

    return s;
  }

  std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> Prepare(
      std::string_view program, std::string_view path) {
    std::string input_program_str = absl::Substitute(program, path);
    auto trace_program = std::make_unique<dynamic_tracing::ir::logical::TracepointDeployment>();
    CHECK(google::protobuf::TextFormat::ParseFromString(input_program_str, trace_program.get()));
    return trace_program;
  }

  std::optional<int> FindFieldIndex(const stirlingpb::TableSchema& schema,
                                    std::string_view field_name) {
    int idx = 0;
    for (const auto& e : schema.elements()) {
      if (e.name() == field_name) {
        return idx;
      }
      ++idx;
    }
    return {};
  }

  void DeployTracepoint(
      std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> trace_program) {
    sole::uuid trace_id = sole::uuid4();
    stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

    // Should deploy.
    stirlingpb::Publish publication;
    ASSERT_OK_AND_ASSIGN(publication, WaitForStatus(trace_id));

    // Check the incremental publication change.
    ASSERT_EQ(publication.published_info_classes_size(), 1);
    info_class_ = publication.published_info_classes(0);

    // Run Stirling data collector.
    ASSERT_OK(stirling_->RunAsThread());

    // Wait to capture some data.
    while (record_batches_.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    ASSERT_OK(stirling_->RemoveTracepoint(trace_id));

    // Should get removed.
    EXPECT_EQ(WaitForStatus(trace_id).code(), px::statuspb::Code::NOT_FOUND);

    stirling_->Stop();
  }

  std::unique_ptr<Stirling> stirling_;
  std::vector<std::unique_ptr<types::ColumnWrapperRecordBatch>> record_batches_;
  stirlingpb::InfoClass info_class_;
};

//-----------------------------------------------------------------------------
// Dynamic Trace API tests
//-----------------------------------------------------------------------------

class DynamicTraceAPITest : public StirlingDynamicTraceBPFTest {
 protected:
  const std::string kBinaryPath =
      BazelRunfilePath("src/stirling/obj_tools/testdata/cc/test_exe_/test_exe");

  static constexpr std::string_view kTracepointDeploymentTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: CPP
      outputs {
        name: "output_table"
        fields: "a"
        fields: "b"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "CanYouFindThis"
          type: LOGICAL
        }
        args {
          id: "a"
          expr: "a"
        }
        args {
          id: "b"
          expr: "b"
        }
        ret_vals {
          id: "retval0"
          expr: "$$0"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "a"
          variable_names: "b"
        }
      }
    }
  }
  )";
};

TEST_F(DynamicTraceAPITest, DynamicTraceAPI) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  // Checking status of non-existent trace should return NOT_FOUND.
  s = stirling_->GetTracepointInfo(trace_id);
  EXPECT_EQ(s.code(), px::statuspb::Code::NOT_FOUND);

  auto trace_program = Prepare(kTracepointDeploymentTxtPB, kBinaryPath);
  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  // Immediately after registering, state should be pending.
  // TODO(oazizi): How can we make sure this is not flaky?
  s = stirling_->GetTracepointInfo(trace_id);
  EXPECT_EQ(s.code(), px::statuspb::Code::RESOURCE_UNAVAILABLE) << s.ToString();

  // Should deploy.
  ASSERT_OK(WaitForStatus(trace_id));

  // TODO(oazizi): Expand test when RegisterTracepoint produces other states.
}

TEST_F(DynamicTraceAPITest, NonExistentBinary) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  auto trace_program = Prepare(kTracepointDeploymentTxtPB, "ThisIsNotARealBinaryPath");
  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  s = WaitForStatus(trace_id);
  EXPECT_EQ(s.code(), px::statuspb::Code::FAILED_PRECONDITION);
}

TEST_F(DynamicTraceAPITest, MissingSymbol) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  auto trace_program = Prepare(kTracepointDeploymentTxtPB, kBinaryPath);

  // Make the tracepoint malformed by trying to trace a non-existent symbol.
  trace_program->mutable_tracepoints(0)
      ->mutable_program()
      ->mutable_probes(0)
      ->mutable_tracepoint()
      ->set_symbol("GoodLuckFindingThis");

  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  s = WaitForStatus(trace_id);
  EXPECT_EQ(s.code(), px::statuspb::Code::INTERNAL);
}

TEST_F(DynamicTraceAPITest, InvalidReference) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  auto trace_program = Prepare(kTracepointDeploymentTxtPB, kBinaryPath);

  // Make the tracepoint malformed by making one of the variable references invalid.
  auto* variable_names = trace_program->mutable_tracepoints(0)
                             ->mutable_program()
                             ->mutable_probes(0)
                             ->mutable_output_actions(0)
                             ->mutable_variable_names(0);
  *variable_names = "invref";

  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  s = WaitForStatus(trace_id);
  EXPECT_EQ(s.code(), px::statuspb::Code::INTERNAL);
}

//-----------------------------------------------------------------------------
// Dynamic Trace Golang tests
//-----------------------------------------------------------------------------

class DynamicTraceGolangTest : public StirlingDynamicTraceBPFTest {
 protected:
  const std::string kBinaryPath =
      BazelRunfilePath("src/stirling/obj_tools/testdata/go/test_go_1_16_binary");
};

TEST_F(DynamicTraceGolangTest, TraceLatencyOnly) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: GOLANG
      outputs {
        name: "output_table"
        fields: "latency"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "main.SaySomethingTo"
          type: LOGICAL
        }
        function_latency { id: "lat0" }
        output_actions {
          output_name: "output_table"
          variable_names: "lat0"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int latency_field_idx,
                              FindFieldIndex(info_class_.schema(), "latency"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_GT(rb[latency_field_idx]->Get<types::Int64Value>(0), 0);
}

TEST_F(DynamicTraceGolangTest, TraceString) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: GOLANG
      outputs {
        name: "output_table"
        fields: "something"
        fields: "name"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "main.SaySomethingTo"
          type: LOGICAL
        }
        args {
          id: "name"
          expr: "name"
        }
        args {
          id: "something"
          expr: "something"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "something"
          variable_names: "name"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int name_field_idx, FindFieldIndex(info_class_.schema(), "name"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int something_field_idx,
                              FindFieldIndex(info_class_.schema(), "something"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[something_field_idx]->Get<types::StringValue>(0), "Hello");
  EXPECT_EQ(rb[name_field_idx]->Get<types::StringValue>(0), "pixienaut");
}

TEST_F(DynamicTraceGolangTest, TraceLongString) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: GOLANG
      outputs {
        name: "output_table"
        fields: "value"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "main.Echo"
          type: LOGICAL
        }
        args {
          id: "arg0"
          expr: "x"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "arg0"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int value_field_idx, FindFieldIndex(info_class_.schema(), "value"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[value_field_idx]->Get<types::StringValue>(0), "This is a loooooooooooo<truncated>");
}

// Tests tracing StructBlob variables.
// TODO(yzhao): Add test for returned StructBlob of OuterStructFunc(). We need to put output
// variable into BPF_PERCPU_ARRAY:
// (https://github.com/iovisor/bcc/blob/master/docs/reference_guide.md#7-bpf_percpu_array)
// to work around the stack size limit.
TEST_F(DynamicTraceGolangTest, TraceStructBlob) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: GOLANG
      outputs {
        name: "output_table"
        fields: "struct_blob"
        fields: "return_struct_blob"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "main.OuterStructFunc"
          type: LOGICAL
        }
        args {
          id: "arg0"
          expr: "x"
        }
        ret_vals {
          id: "retval0"
          expr: "$$0"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "arg0"
          variable_names: "retval0"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);
  DeployTracepoint(std::move(trace_program));

  ASSERT_HAS_VALUE_AND_ASSIGN(int struct_blob_field_idx,
                              FindFieldIndex(info_class_.schema(), "struct_blob"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int ret_field_idx,
                              FindFieldIndex(info_class_.schema(), "return_struct_blob"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(
      rb[struct_blob_field_idx]->Get<types::StringValue>(0),
      R"({"O0":1,"O1":{"M0":{"L0":true,"L1":2,"L2":0},"M1":false,"M2":{"L0":true,"L1":3,"L2":0}}})");
  EXPECT_EQ(rb[ret_field_idx]->Get<types::StringValue>(0), R"({"X":3,"Y":4})");
}

struct ReturnedErrorInterfaceTestCase {
  std::string logical_program;
  std::string expected_output;
};

class ReturnedErrorInterfaceTest
    : public DynamicTraceGolangTest,
      public ::testing::WithParamInterface<ReturnedErrorInterfaceTestCase> {};

TEST_P(ReturnedErrorInterfaceTest, TraceError) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  std::string logical_program = GetParam().logical_program;
  std::string expected_output = GetParam().expected_output;

  auto tracepoint_deployment =
      std::make_unique<dynamic_tracing::ir::logical::TracepointDeployment>();
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(logical_program, tracepoint_deployment.get()));

  tracepoint_deployment->mutable_deployment_spec()->mutable_path_list()->add_paths(kBinaryPath);

  DeployTracepoint(std::move(tracepoint_deployment));

  ASSERT_HAS_VALUE_AND_ASSIGN(int err_field_idx, FindFieldIndex(info_class_.schema(), "err"));

  EXPECT_THAT(record_batches_, SizeIs(1));
  const auto& rb = *record_batches_[0];
  for (size_t i = 0; i < rb[err_field_idx]->Size(); ++i) {
    EXPECT_THAT(std::string(rb[err_field_idx]->Get<types::StringValue>(i)), StrEq(expected_output));
  }
}

constexpr std::string_view kProgramTxtPBTmpl = R"(
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "output_table"
      fields: "err"
    }
    probes {
      name: "probe0"
      tracepoint {
        symbol: "$0"
        type: LOGICAL
      }
      ret_vals {
        id: "retval0"
        expr: "$$0"
      }
      output_actions {
        output_name: "output_table"
        variable_names: "retval0"
      }
    }
  }
}
)";

INSTANTIATE_TEST_SUITE_P(
    NilAndNonNilError, ReturnedErrorInterfaceTest,
    ::testing::Values(
        ReturnedErrorInterfaceTestCase{absl::Substitute(kProgramTxtPBTmpl, "main.ReturnError"),
                                       "{\"X\":3,\"Y\":4}"},
        ReturnedErrorInterfaceTestCase{absl::Substitute(kProgramTxtPBTmpl, "main.ReturnNilError"),
                                       "{\"tab\":0,\"data\":0}"}));

struct TestParam {
  std::string function_symbol;
  std::string value;
};

class DynamicTraceGolangTestWithParam : public DynamicTraceGolangTest,
                                        public ::testing::WithParamInterface<TestParam> {};

TEST_P(DynamicTraceGolangTestWithParam, TraceByteArray) {
  auto params = GetParam();

  // Run tracing target.
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: GOLANG
      outputs {
        name: "output_table"
        fields: "uuid"
        fields: "name"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: ""
          type: LOGICAL
        }
        args {
          id: "arg0"
          expr: "uuid"
        }
        args {
          id: "arg1"
          expr: "name"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "arg0"
          variable_names: "arg1"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);
  trace_program->mutable_tracepoints(0)
      ->mutable_program()
      ->mutable_probes(0)
      ->mutable_tracepoint()
      ->set_symbol(params.function_symbol);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int uuid_field_idx, FindFieldIndex(info_class_.schema(), "uuid"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int name_field_idx, FindFieldIndex(info_class_.schema(), "name"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[uuid_field_idx]->Get<types::StringValue>(0), "000102030405060708090A0B0C0D0E0F");
  EXPECT_EQ(rb[name_field_idx]->Get<types::StringValue>(0), params.value);
}

INSTANTIATE_TEST_SUITE_P(GolangByteArrayTests, DynamicTraceGolangTestWithParam,
                         ::testing::Values(TestParam{"main.BytesToHex", "Bytes"},
                                           TestParam{"main.Uint8ArrayToHex", "Uint8"}));

//-----------------------------------------------------------------------------
// Dynamic Trace C++ tests
//-----------------------------------------------------------------------------

class DynamicTraceCppTest : public StirlingDynamicTraceBPFTest {
 protected:
  const std::string kBinaryPath =
      BazelRunfilePath("src/stirling/obj_tools/testdata/cc/test_exe_/test_exe");
};

TEST_F(DynamicTraceCppTest, BasicTypes) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: CPP
      outputs {
        name: "output_table"
        fields: "a"
        fields: "b"
        fields: "sum"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "CanYouFindThis"
          type: LOGICAL
        }
        args {
          id: "arg0"
          expr: "a"
        }
        args {
          id: "arg1"
          expr: "b"
        }
        ret_vals {
          id: "retval0"
          expr: "$$0"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "arg0"
          variable_names: "arg1"
          variable_names: "retval0"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int a_field_idx, FindFieldIndex(info_class_.schema(), "a"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int b_field_idx, FindFieldIndex(info_class_.schema(), "b"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int sum_field_idx, FindFieldIndex(info_class_.schema(), "sum"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[a_field_idx]->Get<types::Int64Value>(0).val, 3);
  EXPECT_EQ(rb[b_field_idx]->Get<types::Int64Value>(0).val, 4);
  EXPECT_EQ(rb[sum_field_idx]->Get<types::Int64Value>(0).val, 7);
}

class DynamicTraceCppTestWithParam : public DynamicTraceCppTest,
                                     public ::testing::WithParamInterface<std::string> {};

TEST_P(DynamicTraceCppTestWithParam, StructTypes) {
  auto param = GetParam();

  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: CPP
      outputs {
        name: "output_table"
        fields: "x"
        fields: "y"
        fields: "sum"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: ""
          type: LOGICAL
        }
        args {
          id: "arg0"
          expr: "x.a"
        }
        args {
          id: "arg1"
          expr: "y.a"
        }
        ret_vals {
          id: "retval0"
          expr: "$$0.a"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "arg0"
          variable_names: "arg1"
          variable_names: "retval0"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);
  trace_program->mutable_tracepoints(0)
      ->mutable_program()
      ->mutable_probes(0)
      ->mutable_tracepoint()
      ->set_symbol(param);

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  //  ASSERT_HAS_VALUE_AND_ASSIGN(int x_field_idx, FindFieldIndex(info_class_.schema(), "x"));
  //  ASSERT_HAS_VALUE_AND_ASSIGN(int y_field_idx, FindFieldIndex(info_class_.schema(), "y"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int sum_field_idx, FindFieldIndex(info_class_.schema(), "sum"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  //  EXPECT_EQ(rb[x_field_idx]->Get<types::Int64Value>(0).val, 1);
  //  EXPECT_EQ(rb[y_field_idx]->Get<types::Int64Value>(0).val, 4);
  EXPECT_EQ(rb[sum_field_idx]->Get<types::Int64Value>(0).val, 5);
}

INSTANTIATE_TEST_SUITE_P(CppStructTracingTests, DynamicTraceCppTestWithParam,
                         ::testing::Values("ABCSum32", "ABCSum64"));

TEST_F(DynamicTraceCppTest, ArgsOnStackAndRegisters) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "$0"
    }
  }
  tracepoints {
    program {
      language: CPP
      outputs {
        name: "output_table"
        fields: "x_a"
        fields: "x_c"
        fields: "y_a"
        fields: "y_c"
        fields: "z_a"
        fields: "z_c"
        fields: "w_a"
        fields: "w_c"
        fields: "sum_a"
        fields: "sum_c"
      }
      probes {
        name: "probe0"
        tracepoint {
          symbol: "ABCSumMixed"
          type: LOGICAL
        }
        args {
          id: "arg00"
          expr: "x.a"
        }
        args {
          id: "arg01"
          expr: "x.c"
        }
        args {
          id: "arg10"
          expr: "y.a"
        }
        args {
          id: "arg11"
          expr: "y.c"
        }
        args {
          id: "arg20"
          expr: "z_a"
        }
        args {
          id: "arg21"
          expr: "z_c"
        }
        args {
          id: "arg30"
          expr: "w.a"
        }
        args {
          id: "arg31"
          expr: "w.c"
        }
        ret_vals {
          id: "retval00"
          expr: "$$0.a"
        }
        ret_vals {
          id: "retval01"
          expr: "$$0.c"
        }
        output_actions {
          output_name: "output_table"
          variable_names: "arg00"
          variable_names: "arg01"
          variable_names: "arg10"
          variable_names: "arg11"
          variable_names: "arg20"
          variable_names: "arg21"
          variable_names: "arg30"
          variable_names: "arg31"
          variable_names: "retval00"
          variable_names: "retval01"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, kBinaryPath);

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int x_a_field_idx, FindFieldIndex(info_class_.schema(), "x_a"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int x_c_field_idx, FindFieldIndex(info_class_.schema(), "x_c"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int y_a_field_idx, FindFieldIndex(info_class_.schema(), "y_a"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int y_c_field_idx, FindFieldIndex(info_class_.schema(), "y_c"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int z_a_field_idx, FindFieldIndex(info_class_.schema(), "z_a"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int z_c_field_idx, FindFieldIndex(info_class_.schema(), "z_c"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int w_a_field_idx, FindFieldIndex(info_class_.schema(), "w_a"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int w_c_field_idx, FindFieldIndex(info_class_.schema(), "w_c"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int sum_a_field_idx, FindFieldIndex(info_class_.schema(), "sum_a"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int sum_c_field_idx, FindFieldIndex(info_class_.schema(), "sum_c"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[x_a_field_idx]->Get<types::Int64Value>(0).val, 1);
  EXPECT_EQ(rb[x_c_field_idx]->Get<types::Int64Value>(0).val, 3);
  EXPECT_EQ(rb[y_a_field_idx]->Get<types::Int64Value>(0).val, 4);
  EXPECT_EQ(rb[y_c_field_idx]->Get<types::Int64Value>(0).val, 6);
  EXPECT_EQ(rb[z_a_field_idx]->Get<types::Int64Value>(0).val, 7);
  EXPECT_EQ(rb[z_c_field_idx]->Get<types::Int64Value>(0).val, 9);
  EXPECT_EQ(rb[w_a_field_idx]->Get<types::Int64Value>(0).val, 10);
  EXPECT_EQ(rb[w_c_field_idx]->Get<types::Int64Value>(0).val, 12);
  EXPECT_EQ(rb[sum_a_field_idx]->Get<types::Int64Value>(0).val, 22);
  EXPECT_EQ(rb[sum_c_field_idx]->Get<types::Int64Value>(0).val, 30);
}

//-----------------------------------------------------------------------------
// Dynamic Trace library tests
//-----------------------------------------------------------------------------

class DynamicTraceSharedLibraryTest : public StirlingDynamicTraceBPFTest {
 protected:
  const std::string kBinaryPath = BazelRunfilePath("src/stirling/testing/dns/dns_hammer");
};

TEST_F(DynamicTraceSharedLibraryTest, GetAddrInfo) {
  BinaryRunner trace_target;
  trace_target.Run(kBinaryPath);

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    path_list {
      paths: "/lib/x86_64-linux-gnu/libc.so.6"
    }
  }
  tracepoints {
    table_name: "foo"
    program {
      language: CPP
      outputs {
        name: "dns_latency_table"
        fields: "latency"
      }
      probes {
        name: "dns_latency_tracepoint"
        tracepoint {
          symbol: "getaddrinfo"
        }
        function_latency {
          id: "lat0"
        }
        output_actions {
          output_name: "dns_latency_table"
          variable_names: "lat0"
        }
      }
    }
  }
  )";

  auto trace_program = Prepare(kProgramTxtPB, "");

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int latency_idx, FindFieldIndex(info_class_.schema(), "latency"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_GT(rb[latency_idx]->Get<types::Int64Value>(0).val, 0);
}

class JavaDNSHammerContainer : public ContainerRunner {
 public:
  JavaDNSHammerContainer()
      : ContainerRunner(BazelRunfilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  // Image is created through bazel rules, and stored as a tar file. It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/dns/dns_hammer_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "dns_hammer";
  static constexpr std::string_view kReadyMessage = "";
};

TEST_F(DynamicTraceSharedLibraryTest, GetAddrInfoInsideContainer) {
  // Run tracing target.
  JavaDNSHammerContainer trace_target_;
  trace_target_.Run(std::chrono::seconds{60});

  constexpr std::string_view kProgramTxtPB = R"(
  deployment_spec {
    shared_object {
      name: "libc"
      upid: {
        pid: $0
      }
    }
  }
  tracepoints {
    table_name: "foo"
    program {
      language: CPP
      outputs {
        name: "dns_latency_table"
        fields: "latency"
      }
      probes {
        name: "dns_latency_tracepoint"
        tracepoint {
          symbol: "getaddrinfo"
        }
        function_latency {
          id: "lat0"
        }
        output_actions {
          output_name: "dns_latency_table"
          variable_names: "lat0"
        }
      }
    }
  }
  )";
  auto trace_program = Prepare(kProgramTxtPB, std::to_string(trace_target_.process_pid()));

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int latency_idx, FindFieldIndex(info_class_.schema(), "latency"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int pid_idx, FindFieldIndex(info_class_.schema(), "upid"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_GT(rb[latency_idx]->Get<types::Int64Value>(0).val, 0);

  // Make sure the captured data comes from within the container by checking the PID.
  EXPECT_EQ(rb[pid_idx]->Get<types::UInt128Value>(0).High64(), trace_target_.process_pid());
}

}  // namespace stirling
}  // namespace px
