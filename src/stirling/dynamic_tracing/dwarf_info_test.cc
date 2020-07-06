#include <string>

#include <google/protobuf/util/message_differencer.h>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;

constexpr std::string_view kEntryProbeIn = R"(
probes: {
  trace_point: {
    binary_path: "$0"
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
}
)";

constexpr std::string_view kEntryProbeOut = R"(
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "tgid"
    type: INT32
    builtin: TGID
  }
  vars {
    name: "tgid_start_time"
    type: UINT64
    builtin: TGID_START_TIME
  }
  vars {
    name: "goid"
    type: INT32
    builtin: GOID
  }
  vars {
    name: "ktime_ns"
    type: UINT64
    builtin: KTIME
  }
  vars {
    name: "arg0"
    type: INT
    memory: {
      base: "sp"
      offset: 8
    }
  }
  vars {
    name: "arg1"
    type: INT
    memory: {
      base: "sp"
      offset: 24
    }
  }
  vars {
    name: "arg2"
    type: INT
    memory: {
      base: "sp"
      offset: 32
    }
  }
  vars {
    name: "arg3"
    type: BOOL
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg4"
    type: BOOL
    memory: {
      base: "sp"
      offset: 17
    }
  }
  vars {
    name: "arg5"
    type: BOOL
    memory: {
      base: "sp"
      offset: 20
    }
  }
}
)";

constexpr std::string_view kReturnProbeIn = R"(
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: RETURN
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

constexpr std::string_view kReturnProbeOut = R"(
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "tgid"
    type: INT32
    builtin: TGID
  }
  vars {
    name: "tgid_start_time"
    type: UINT64
    builtin: TGID_START_TIME
  }
  vars {
    name: "goid"
    type: INT32
    builtin: GOID
  }
  vars {
    name: "ktime_ns"
    type: UINT64
    builtin: KTIME
  }
  vars {
    name: "retval0"
    type: INT
    memory: {
      base: "sp"
      offset: 48
    }
  }
vars {
    name: "retval1"
    type: BOOL
    memory: {
      base: "sp"
      offset: 56
    }
  }
}
)";

constexpr std::string_view kNestedArgProbeIn = R"(
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.PointerWrapperWrapperWrapperFunc"
    type: ENTRY
  }
  args {
    id: "arg0"
    expr: "p.Ptr.Val.Ptr"
  }
  args {
    id: "arg1"
    expr: "p.Ptr.Val.V0"
  }
}
)";

constexpr std::string_view kNestedArgProbeOut = R"(
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.PointerWrapperWrapperWrapperFunc"
    type: ENTRY
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "tgid"
    type: INT32
    builtin: TGID
  }
  vars {
    name: "tgid_start_time"
    type: UINT64
    builtin: TGID_START_TIME
  }
  vars {
    name: "goid"
    type: INT32
    builtin: GOID
  }
  vars {
    name: "ktime_ns"
    type: UINT64
    builtin: KTIME
  }
  vars {
    name: "arg0_D_Ptr_X_"
    type: VOID_POINTER
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg0_D_Ptr_X__D_Val_D_Ptr_X_"
    type: VOID_POINTER
    memory: {
      base: "arg0_D_Ptr_X_"
      offset: 40
    }
  }
  vars {
    name: "arg0"
    type: INT
    memory: {
      base: "arg0_D_Ptr_X__D_Val_D_Ptr_X_"
      offset: 0
    }
  }
  vars {
    name: "arg1_D_Ptr_X_"
    type: VOID_POINTER
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg1"
    type: INT64
    memory: {
      base: "arg1_D_Ptr_X_"
      offset: 16
    }
  }
}
)";

constexpr std::string_view kActionProbeIn = R"(
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  args {
    id: "arg0"
    expr: "i1"
  }
  args {
    id: "arg1"
    expr: "b1"
  }
  args {
    id: "arg2"
    expr: "b2.B0"
  }
  stash_map_actions {
    map_name: "my_stash"
    key_expr: "goid"
    value_variable_name: "arg0"
    value_variable_name: "arg1"
  }
  output_actions {
    output_name: "out_table"
    variable_name: "arg0"
    variable_name: "arg1"
    variable_name: "arg2"
  }
}
)";

constexpr std::string_view kActionProbeOut = R"(
structs {
  name: "my_stash_value_t"
  fields {
    name: "my_stash_arg0"
    type { scalar: INT }
  }
  fields {
    name: "my_stash_arg1"
    type { scalar: BOOL }
  }
}
structs {
  name: "out_table_value_t"
  fields {
    name: "out_table_tgid"
    type { scalar: INT32 }
  }
  fields {
    name: "out_table_tgid_start_time"
    type { scalar: UINT64 }
  }
  fields {
    name: "out_table_goid"
    type { scalar: INT32 }
  }
  fields {
    name: "out_table_ktime_ns"
    type { scalar: UINT64 }
  }
  fields {
    name: "out_table_arg0"
    type { scalar: INT }
  }
  fields {
    name: "out_table_arg1"
    type { scalar: BOOL }
  }
  fields {
    name: "out_table_arg2"
    type { scalar: BOOL }
  }
}
maps {
  name: "my_stash"
  key_type { scalar: UINT64 }
  value_type { struct_type: "my_stash_value_t" }
}
outputs {
  name: "out_table"
  type { struct_type: "out_table_value_t" }
}
probes: {
  trace_point: {
    binary_path: "$0"
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    name: "sp"
    type: VOID_POINTER
    reg: SP
  }
  vars {
    name: "tgid"
    type: INT32
    builtin: TGID
  }
  vars {
    name: "tgid_start_time"
    type: UINT64
    builtin: TGID_START_TIME
  }
  vars {
    name: "goid"
    type: INT32
    builtin: GOID
  }
  vars {
    name: "ktime_ns"
    type: UINT64
    builtin: KTIME
  }
  vars {
    name: "arg0"
    type: INT
    memory: {
      base: "sp"
      offset: 8
    }
  }
  vars {
    name: "arg1"
    type: BOOL
    memory: {
      base: "sp"
      offset: 16
    }
  }
  vars {
    name: "arg2"
    type: BOOL
    memory: {
      base: "sp"
      offset: 17
    }
  }
  st_vars {
    name: "my_stash_value"
    type: "my_stash_value_t"
    variable_names {
      name: "arg0"
    }
    variable_names {
      name: "arg1"
    }
  }
  st_vars {
    name: "out_table_value"
    type: "out_table_value_t"
    variable_names {
      name: "tgid"
    }
    variable_names {
      name: "tgid_start_time"
    }
    variable_names {
      name: "goid"
    }
    variable_names {
      name: "ktime_ns"
    }
    variable_names {
      name: "arg0"
    }
    variable_names {
      name: "arg1"
    }
    variable_names {
      name: "arg2"
    }
  }
  map_stash_actions {
    map_name: "my_stash"
    key_variable_name: "goid"
    value_variable_name: "my_stash_value"
  }
  output_actions {
    perf_buffer_name: "out_table"
    variable_name: "out_table_value"
  }
}
)";

struct DwarfInfoTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class DwarfInfoTest : public ::testing::TestWithParam<DwarfInfoTestParam> {
 protected:
  DwarfInfoTest() : kGoBinaryPath(pl::testing::TestFilePath(FLAGS_dummy_go_binary)) {}

  std::string kGoBinaryPath;
};

TEST_P(DwarfInfoTest, Transform) {
  DwarfInfoTestParam p = GetParam();

  std::string input_str = absl::Substitute(p.input, kGoBinaryPath);
  ir::logical::Program input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(std::string(input_str), &input_program));

  std::string expected_output_str = absl::Substitute(p.expected_output, kGoBinaryPath);
  ir::physical::Program expected_output;
  TextFormat::ParseFromString(std::string(expected_output_str), &expected_output);

  ASSERT_OK_AND_ASSIGN(ir::physical::Program program, AddDwarves(input_program));

  MessageDifferencer message_differencer;
  std::string diff_out;
  message_differencer.ReportDifferencesToString(&diff_out);
  EXPECT_TRUE(message_differencer.Compare(program, expected_output)) << diff_out;
}

INSTANTIATE_TEST_SUITE_P(DwarfInfoTestSuite, DwarfInfoTest,
                         ::testing::Values(DwarfInfoTestParam{kEntryProbeIn, kEntryProbeOut},
                                           DwarfInfoTestParam{kReturnProbeIn, kReturnProbeOut},
                                           DwarfInfoTestParam{kNestedArgProbeIn,
                                                              kNestedArgProbeOut},
                                           DwarfInfoTestParam{kActionProbeIn, kActionProbeOut}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
