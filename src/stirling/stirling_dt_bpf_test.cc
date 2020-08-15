#include <functional>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

//-----------------------------------------------------------------------------
// Test fixture and shared code
//-----------------------------------------------------------------------------

class StirlingDynamicTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    registry->RegisterOrDie<SocketTraceConnector>("socket_trace_connector");

    // Make Stirling.
    stirling_ = Stirling::Create(std::move(registry));

    // Set dummy callbacks for agent function.
    stirling_->RegisterDataPushCallback(std::bind(&StirlingDynamicTraceBPFTest::AppendData, this,
                                                  std::placeholders::_1, std::placeholders::_2,
                                                  std::placeholders::_3));
  }

  void AppendData(uint64_t table_id, types::TabletID tablet_id,
                  std::unique_ptr<types::ColumnWrapperRecordBatch> record_batch) {
    PL_UNUSED(table_id);
    PL_UNUSED(tablet_id);
    record_batches_.push_back(std::move(record_batch));
  }

  StatusOr<stirlingpb::Publish> WaitForStatus(sole::uuid trace_id) {
    StatusOr<stirlingpb::Publish> s;
    do {
      s = stirling_->GetTracepointInfo(trace_id);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (!s.ok() && s.code() == pl::statuspb::Code::RESOURCE_UNAVAILABLE);

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

    // Subscribe to the new info class.
    ASSERT_OK(stirling_->SetSubscription(pl::stirling::SubscribeToAllInfoClasses(publication)));

    // Run Stirling data collector.
    ASSERT_OK(stirling_->RunAsThread());

    // Wait to capture some data.
    while (record_batches_.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    ASSERT_OK(stirling_->RemoveTracepoint(trace_id));

    // Should get removed.
    EXPECT_EQ(WaitForStatus(trace_id).code(), pl::statuspb::Code::NOT_FOUND);

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
      pl::testing::BazelBinTestFilePath("src/stirling/obj_tools/testdata/dummy_exe");

  static constexpr std::string_view kTracepointDeployment = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "a"
        variable_name: "b"
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
  EXPECT_EQ(s.code(), pl::statuspb::Code::NOT_FOUND);

  auto trace_program = Prepare(kTracepointDeployment, kBinaryPath);
  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  // Immediately after registering, state should be pending.
  // TODO(oazizi): How can we make sure this is not flaky?
  s = stirling_->GetTracepointInfo(trace_id);
  EXPECT_EQ(s.code(), pl::statuspb::Code::RESOURCE_UNAVAILABLE) << s.ToString();

  // Should deploy.
  ASSERT_OK(WaitForStatus(trace_id));

  // TODO(oazizi): Expand test when RegisterTracepoint produces other states.
}

TEST_F(DynamicTraceAPITest, NonExistentBinary) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  auto trace_program = Prepare(kTracepointDeployment, "ThisIsNotARealBinaryPath");
  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  s = WaitForStatus(trace_id);
  EXPECT_EQ(s.code(), pl::statuspb::Code::FAILED_PRECONDITION);
}

TEST_F(DynamicTraceAPITest, MissingSymbol) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  auto trace_program = Prepare(kTracepointDeployment, kBinaryPath);

  // Make the tracepoint malformed by trying to trace a non-existent symbol.
  trace_program->mutable_tracepoints(0)
      ->mutable_program()
      ->mutable_probes(0)
      ->mutable_tracepoint()
      ->set_symbol("GoodLuckFindingThis");

  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  s = WaitForStatus(trace_id);
  EXPECT_EQ(s.code(), pl::statuspb::Code::INTERNAL);
}

TEST_F(DynamicTraceAPITest, InvalidReference) {
  StatusOr<stirlingpb::Publish> s;

  sole::uuid trace_id = sole::uuid4();

  auto trace_program = Prepare(kTracepointDeployment, kBinaryPath);

  // Make the tracepoint malformed by making one of the variable references invalid.
  auto* variable_name = trace_program->mutable_tracepoints(0)
                            ->mutable_program()
                            ->mutable_probes(0)
                            ->mutable_output_actions(0)
                            ->mutable_variable_name(0);
  *variable_name = "invref";

  stirling_->RegisterTracepoint(trace_id, std::move(trace_program));

  s = WaitForStatus(trace_id);
  EXPECT_EQ(s.code(), pl::statuspb::Code::INTERNAL);
}

//-----------------------------------------------------------------------------
// Dynamic Trace Golang tests
//-----------------------------------------------------------------------------

class DynamicTraceGolangTest : public StirlingDynamicTraceBPFTest {
 protected:
  const std::string kBinaryPath = pl::testing::BazelBinTestFilePath(
      "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary");

  void StartTraceTarget() {
    // Run tracing target.
    SubProcess process;
    ASSERT_OK(fs::Exists(kBinaryPath));
    ASSERT_OK(process.Start({kBinaryPath}));
  }
};

TEST_F(DynamicTraceGolangTest, TraceLatencyOnly) {
  StartTraceTarget();

  constexpr std::string_view kProgram = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "lat0"
      }
    }
  }
}
)";

  auto trace_program = Prepare(kProgram, kBinaryPath);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the two columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int latency_field_idx,
                              FindFieldIndex(info_class_.schema(), "latency"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_GT(rb[latency_field_idx]->Get<types::Int64Value>(0), 0);
}

TEST_F(DynamicTraceGolangTest, TraceString) {
  StartTraceTarget();

  constexpr std::string_view kProgram = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "something"
        variable_name: "name"
      }
    }
  }
}
)";

  auto trace_program = Prepare(kProgram, kBinaryPath);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the two columns we want.
  ASSERT_HAS_VALUE_AND_ASSIGN(int name_field_idx, FindFieldIndex(info_class_.schema(), "name"));
  ASSERT_HAS_VALUE_AND_ASSIGN(int something_field_idx,
                              FindFieldIndex(info_class_.schema(), "something"));

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[something_field_idx]->Get<types::StringValue>(0), "Hello");
  EXPECT_EQ(rb[name_field_idx]->Get<types::StringValue>(0), "pixienaut");
}

struct TestParam {
  std::string function_symbol;
  std::string value;
};

class DynamicTraceGolangTestWithParam : public DynamicTraceGolangTest,
                                        public ::testing::WithParamInterface<TestParam> {};

TEST_P(DynamicTraceGolangTestWithParam, TraceByteArray) {
  auto params = GetParam();

  // Run tracing target.
  StartTraceTarget();

  constexpr std::string_view kProgram = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "arg0"
        variable_name: "arg1"
      }
    }
  }
}
)";

  auto trace_program = Prepare(kProgram, kBinaryPath);
  trace_program->mutable_tracepoints(0)
      ->mutable_program()
      ->mutable_probes(0)
      ->mutable_tracepoint()
      ->set_symbol(params.function_symbol);
  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the two columns we want.
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
      pl::testing::BazelBinTestFilePath("src/stirling/obj_tools/testdata/dummy_exe");

  void StartTraceTarget() {
    // Run tracing target.
    SubProcess process;
    ASSERT_OK(fs::Exists(kBinaryPath));
    ASSERT_OK(process.Start({kBinaryPath}));
  }
};

TEST_F(DynamicTraceCppTest, BasicTypes) {
  StartTraceTarget();

  constexpr std::string_view kProgram = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "arg0"
        variable_name: "arg1"
        variable_name: "retval0"
      }
    }
  }
}
)";

  auto trace_program = Prepare(kProgram, kBinaryPath);

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the two columns we want.
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

  StartTraceTarget();

  constexpr std::string_view kProgram = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "arg0"
        variable_name: "arg1"
        variable_name: "retval0"
      }
    }
  }
}
)";

  auto trace_program = Prepare(kProgram, kBinaryPath);
  trace_program->mutable_tracepoints(0)
      ->mutable_program()
      ->mutable_probes(0)
      ->mutable_tracepoint()
      ->set_symbol(param);

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the two columns we want.
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
  StartTraceTarget();

  constexpr std::string_view kProgram = R"(
deployment_spec {
  path: "$0"
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
        variable_name: "arg00"
        variable_name: "arg01"
        variable_name: "arg10"
        variable_name: "arg11"
        variable_name: "arg20"
        variable_name: "arg21"
        variable_name: "arg30"
        variable_name: "arg31"
        variable_name: "retval00"
        variable_name: "retval01"
      }
    }
  }
}
)";

  auto trace_program = Prepare(kProgram, kBinaryPath);

  DeployTracepoint(std::move(trace_program));

  // Get field indexes for the two columns we want.
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

}  // namespace stirling
}  // namespace pl
