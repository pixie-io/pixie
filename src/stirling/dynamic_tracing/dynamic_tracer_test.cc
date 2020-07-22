#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

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
  DynamicTracerTest() : binary_path_(pl::testing::BazelBinTestFilePath(kBinaryPath)) {}

  std::string binary_path_;
};

TEST_P(DynamicTracerTest, DISABLED_Compile) {
  DynamicTracerTestParam p = GetParam();

  std::string input_program_str = absl::Substitute(p.input, binary_path_);
  ir::logical::Program input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  std::string expected_output = absl::Substitute(p.expected_output, binary_path_);

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, CompileProgram(input_program));

  ASSERT_EQ(bcc_program.code, expected_output);
}

INSTANTIATE_TEST_SUITE_P(DynamicTracerTestSuite, DynamicTracerTest,
                         ::testing::Values(DynamicTracerTestParam{kLogicalProgramSpec,
                                                                  kBCCProgram}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
