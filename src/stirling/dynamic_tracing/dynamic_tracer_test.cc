#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::testing::proto::EqualsProto;

constexpr std::string_view kLogicalProgramSpec = R"(
probes: {
  name: "probe0"
  trace_point: {
    binary_path: "$0"
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
  args {
    id: "arg3"
    expr: "b1"
  }
  args {
    id: "arg4"
    expr: "b2.B0"
  }
  args {
    id: "arg5"
    expr: "b2.B3"
  }
  ret_vals {
    id: "retval0"
    index: 6
  }
  ret_vals {
    id: "retval1"
    index: 7
  }
}
)";

constexpr std::string_view kBCCProgram = R"(
yo
)";

struct DynamicTracerTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class DynamicTracerTest : public ::testing::TestWithParam<DynamicTracerTestParam> {
 protected:
  DynamicTracerTest() : kGoBinaryPath(pl::testing::TestFilePath(FLAGS_dummy_go_binary)) {}

  std::string kGoBinaryPath;
};

TEST_P(DynamicTracerTest, DISABLED_Compile) {
  DynamicTracerTestParam p = GetParam();

  std::string input_program_str = absl::Substitute(p.input, kGoBinaryPath);
  ir::logical::Program input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  std::string expected_output = absl::Substitute(p.expected_output, kGoBinaryPath);

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, CompileProgram(input_program));

  ASSERT_EQ(bcc_program.code, expected_output);
}

INSTANTIATE_TEST_SUITE_P(DynamicTracerTestSuite, DynamicTracerTest,
                         ::testing::Values(DynamicTracerTestParam{kLogicalProgramSpec,
                                                                  kBCCProgram}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
