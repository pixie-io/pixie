#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/probe_transformer.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::testing::proto::EqualsProto;

constexpr std::string_view kLogicalProgram = R"(
binary_path: "$0"
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

constexpr std::string_view kTransformedProgram = R"proto(
binary_path: "$0"
outputs {
  name: "probe0_table"
}
maps {
  name: "pid_goid_map"
}
maps {
  name: "probe0_argstash"
}
probes {
  name: "probe_entry_runtime_casgstatus"
  trace_point {
    symbol: "runtime.casgstatus"
    type: ENTRY
  }
  consts {
    name: "kGRunningState"
    type: INT64
    constant: "2"
  }
  args {
    id: "goid_"
    expr: "gp.goid"
  }
  args {
    id: "newval"
    expr: "newval"
  }
  map_stash_actions {
    map_name: "pid_goid_map"
    key: TGID_PID
    value_variable_name: "goid_"
    cond {
      op: EQUAL
      vars: "newval"
      vars: "kGRunningState"
    }
  }
}
probes {
  name: "probe0_entry"
  trace_point {
    symbol: "main.MixedArgTypes"
    type: ENTRY
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
  map_stash_actions {
    map_name: "probe0_argstash"
    key: GOID
    value_variable_name: "arg0"
    value_variable_name: "arg1"
    value_variable_name: "arg2"
    value_variable_name: "arg3"
    value_variable_name: "arg4"
    value_variable_name: "arg5"
  }
}
probes {
  name: "probe0_return"
  trace_point {
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  map_vals {
    map_name: "probe0_argstash"
    key_expr: "goid"
    value_ids: "arg0"
    value_ids: "arg1"
    value_ids: "arg2"
    value_ids: "arg3"
    value_ids: "arg4"
    value_ids: "arg5"
  }
  ret_vals {
    id: "retval0"
    index: 6
  }
  ret_vals {
    id: "retval1"
    index: 7
  }
  output_actions {
    output_name: "probe0_table"
    variable_name: "arg0"
    variable_name: "arg1"
    variable_name: "arg2"
    variable_name: "arg3"
    variable_name: "arg4"
    variable_name: "arg5"
    variable_name: "retval0"
    variable_name: "retval1"
  }
}
)proto";

struct ProbeGenTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class ProbeGenTest : public ::testing::TestWithParam<ProbeGenTestParam> {
 protected:
  ProbeGenTest() : kGoBinaryPath(pl::testing::TestFilePath(FLAGS_dummy_go_binary)) {}

  std::string kGoBinaryPath;
};

TEST_P(ProbeGenTest, Transform) {
  ProbeGenTestParam p = GetParam();

  std::string input_program_str = absl::Substitute(p.input, kGoBinaryPath);
  ir::logical::Program input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  std::string expected_output = absl::Substitute(p.expected_output, kGoBinaryPath);

  ASSERT_OK_AND_THAT(TransformLogicalProgram(input_program), EqualsProto(expected_output));
}

INSTANTIATE_TEST_SUITE_P(ProbeGenTestSuite, ProbeGenTest,
                         ::testing::Values(ProbeGenTestParam{kLogicalProgram,
                                                             kTransformedProgram}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
