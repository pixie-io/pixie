#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::testing::proto::EqualsProto;
using ::testing::Field;
using ::testing::SizeIs;

constexpr std::string_view kLogicalProgramSpec = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
outputs {
  name: "probe_output"
  fields: "f1"
  fields: "f2"
  fields: "f3"
  fields: "f4"
}
probes: {
  name: "probe0"
  trace_point: {
    symbol: "main.MixedArgTypes"
    type: LOGICAL
  }
  args {
    id: "arg0"
    expr: "i1"
  }
    args {
    id: "arg1"
    expr: "i2"
  }
  args {
    id: "arg2"
    expr: "i3"
  }
  ret_vals {
    id: "retval0"
    expr: "$$6"
  }
  output_actions {
    output_name: "probe_output"
    variable_name: "arg0"
    variable_name: "arg1"
    variable_name: "arg2"
    variable_name: "retval0"
  }
}
)";

TEST(DynamicTracerTest, Compile) {
  std::string input_program_str = absl::Substitute(
      kLogicalProgramSpec, pl::testing::BazelBinTestFilePath(kBinaryPath).string());
  ir::logical::Program input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, CompileProgram(input_program));

  ASSERT_THAT(bcc_program.uprobe_specs, SizeIs(4));

  const auto& spec = bcc_program.uprobe_specs[0];

  EXPECT_THAT(spec, Field(&UProbeSpec::binary_path, ::testing::EndsWith("dummy_go_binary")));
  EXPECT_THAT(spec, Field(&UProbeSpec::symbol, "runtime.casgstatus"));
  EXPECT_THAT(spec, Field(&UProbeSpec::attach_type, bpf_tools::BPFProbeAttachType::kEntry));
  EXPECT_THAT(spec, Field(&UProbeSpec::probe_fn, "probe_entry_runtime_casgstatus"));

  ASSERT_THAT(bcc_program.perf_buffer_specs, SizeIs(1));

  const auto& perf_buffer_name = bcc_program.perf_buffer_specs[0].name;
  const auto& perf_buffer_output = bcc_program.perf_buffer_specs[0].output;

  EXPECT_THAT(perf_buffer_name, "probe_output");
  EXPECT_THAT(perf_buffer_output, EqualsProto(
                                      R"proto(
                                      name: "probe_output_value_t"
                                      fields {
                                        name: "tgid__"
                                        type {
                                          scalar: INT32
                                        }
                                      }
                                      fields {
                                        name: "tgid_start_time__"
                                        type {
                                          scalar: UINT64
                                        }
                                      }
                                      fields {
                                        name: "ktime_ns__"
                                        type {
                                          scalar: UINT64
                                        }
                                      }
                                      fields {
                                        name: "goid__"
                                        type {
                                          scalar: INT64
                                        }
                                      }
                                      fields {
                                        name: "f1"
                                        type {
                                          scalar: INT
                                        }
                                      }
                                      fields {
                                        name: "f2"
                                        type {
                                          scalar: INT
                                        }
                                      }
                                      fields {
                                        name: "f3"
                                        type {
                                          scalar: INT
                                        }
                                      }
                                      fields {
                                        name: "f4"
                                        type {
                                          scalar: INT
                                        }
                                      }
                                      )proto"));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
