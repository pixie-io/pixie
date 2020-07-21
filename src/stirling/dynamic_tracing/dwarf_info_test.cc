#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::testing::proto::EqualsProto;

constexpr std::string_view kEntryProbeIn = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
probes: {
  trace_point: {
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
binary_spec {
  path: "$0"
  language: GOLANG
}
probes {
  trace_point {
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "ktime_ns"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      memory {
        base: "sp"
        offset: 8
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: INT
      memory {
        base: "sp"
        offset: 24
      }
    }
  }
  vars {
    scalar_var {
      name: "arg2"
      type: INT
      memory {
        base: "sp"
        offset: 32
      }
    }
  }
  vars {
    scalar_var {
      name: "arg3"
      memory {
        base: "sp"
        offset: 16
      }
    }
  }
  vars {
    scalar_var {
      name: "arg4"
      memory {
        base: "sp"
        offset: 17
      }
    }
  }
  vars {
    scalar_var {
      name: "arg5"
      memory {
        base: "sp"
        offset: 20
      }
    }
  }
}
)";

constexpr std::string_view kReturnProbeIn = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
probes: {
  trace_point: {
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
binary_spec {
  path: "$0"
  language: GOLANG
}
probes {
  trace_point {
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  vars {
    scalar_var {
      name: "sp"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "ktime_ns"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "retval0"
      type: INT
      memory {
        base: "sp"
        offset: 48
      }
    }
  }
  vars {
    scalar_var {
      name: "retval1"
      memory {
        base: "sp"
        offset: 56
      }
    }
  }
}
)";

constexpr std::string_view kNestedArgProbeIn = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
probes: {
  trace_point: {
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
binary_spec {
  path: "$0"
  language: GOLANG
}
probes {
  trace_point {
    symbol: "main.PointerWrapperWrapperWrapperFunc"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "ktime_ns"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0_D_Ptr_X_"
      type: VOID_POINTER
      memory {
        base: "sp"
        offset: 16
      }
    }
  }
  vars {
    scalar_var {
      name: "arg0_D_Ptr_X__D_Val_D_Ptr_X_"
      type: VOID_POINTER
      memory {
        base: "arg0_D_Ptr_X_"
        offset: 40
      }
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      memory {
        base: "arg0_D_Ptr_X__D_Val_D_Ptr_X_"
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1_D_Ptr_X_"
      type: VOID_POINTER
      memory {
        base: "sp"
        offset: 16
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: INT64
      memory {
        base: "arg1_D_Ptr_X_"
        offset: 16
      }
    }
  }
}
)";

constexpr std::string_view kActionProbeIn = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
maps {
  name: "my_stash"
}
outputs {
  name: "out_table"
  fields: "arg0"
  fields: "arg1"
  fields: "arg2"
}
outputs {
  name: "out_table2"
  fields: "arg0"
  fields: "arg1"
}
probes: {
  trace_point: {
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
  map_stash_actions {
    map_name: "my_stash"
    key: GOID
    value_variable_name: "arg0"
    value_variable_name: "arg1"
    cond {}
  }
  output_actions {
    output_name: "out_table"
    variable_name: "arg0"
    variable_name: "arg1"
    variable_name: "arg2"
  }
}
probes: {
  trace_point: {
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  map_vals {
    map_name: "my_stash"
    key: GOID
    value_ids: "arg0"
    value_ids: "arg1"
  }
  output_actions {
    output_name: "out_table2"
    variable_name: "arg0"
    variable_name: "arg1"
  }
}
)";

constexpr std::string_view kActionProbeOut = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
structs {
  name: "my_stash_value_t"
  fields {
    name: "arg0"
    type {
      scalar: INT
    }
  }
  fields {
    name: "arg1"
    type {
      scalar: BOOL
    }
  }
}
structs {
  name: "out_table_value_t"
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
    name: "arg0"
    type {
      scalar: INT
    }
  }
  fields {
    name: "arg1"
    type {
      scalar: BOOL
    }
  }
  fields {
    name: "arg2"
    type {
      scalar: BOOL
    }
  }
}
structs {
  name: "out_table2_value_t"
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
    name: "arg0"
    type {
      scalar: INT
    }
  }
  fields {
    name: "arg1"
    type {
      scalar: BOOL
    }
  }
}
maps {
  name: "my_stash"
  key_type {
    scalar: UINT64
  }
  value_type {
    struct_type: "my_stash_value_t"
  }
}
outputs {
  name: "out_table"
  fields: "arg0"
  fields: "arg1"
  fields: "arg2"
  struct_type: "out_table_value_t"
}
outputs {
  name: "out_table2"
  fields: "arg0"
  fields: "arg1"
  struct_type: "out_table2_value_t"
}
probes {
  trace_point {
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "ktime_ns"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      memory {
        base: "sp"
        offset: 8
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      memory {
        base: "sp"
        offset: 16
      }
    }
  }
  vars {
    scalar_var {
      name: "arg2"
      memory {
        base: "sp"
        offset: 17
      }
    }
  }
  vars {
    struct_var {
      name: "my_stash_value"
      type: "my_stash_value_t"
      field_assignments {
        field_name: "arg0"
        variable_name: "arg0"
      }
      field_assignments {
        field_name: "arg1"
        variable_name: "arg1"
      }
    }
  }
  vars {
    struct_var {
      name: "out_table_value"
      type: "out_table_value_t"
      field_assignments {
        field_name: "tgid__"
        variable_name: "tgid"
      }
      field_assignments {
        field_name: "tgid_start_time__"
        variable_name: "tgid_start_time"
      }
      field_assignments {
        field_name: "ktime_ns__"
        variable_name: "ktime_ns"
      }
      field_assignments {
        field_name: "goid__"
        variable_name: "goid"
      }
      field_assignments {
        field_name: "arg0"
        variable_name: "arg0"
      }
      field_assignments {
        field_name: "arg1"
        variable_name: "arg1"
      }
      field_assignments {
        field_name: "arg2"
        variable_name: "arg2"
      }
    }
  }
  map_stash_actions {
    map_name: "my_stash"
    key_variable_name: "goid"
    value_variable_name: "my_stash_value"
    cond {
    }
  }
  output_actions {
    perf_buffer_name: "out_table"
    variable_name: "out_table_value"
  }
}
probes {
  trace_point {
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  vars {
    scalar_var {
      name: "sp"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "ktime_ns"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    map_var {
      name: "my_stash_ptr"
      type: "my_stash_value_t"
      map_name: "my_stash"
      key_variable_name: "goid"
    }
  }
  vars {
    member_var {
      name: "arg0"
      type: INT
      struct_base: "my_stash_ptr"
      is_struct_base_pointer: true
      field: "arg0"
    }
  }
  vars {
    member_var {
      name: "arg1"
      struct_base: "my_stash_ptr"
      is_struct_base_pointer: true
      field: "arg1"
    }
  }
  vars {
    struct_var {
      name: "out_table2_value"
      type: "out_table2_value_t"
      field_assignments {
        field_name: "tgid__"
        variable_name: "tgid"
      }
      field_assignments {
        field_name: "tgid_start_time__"
        variable_name: "tgid_start_time"
      }
      field_assignments {
        field_name: "ktime_ns__"
        variable_name: "ktime_ns"
      }
      field_assignments {
        field_name: "goid__"
        variable_name: "goid"
      }
      field_assignments {
        field_name: "arg0"
        variable_name: "arg0"
      }
      field_assignments {
        field_name: "arg1"
        variable_name: "arg1"
      }
    }
  }
  output_actions {
    perf_buffer_name: "out_table2"
    variable_name: "out_table2_value"
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
  ASSERT_OK_AND_THAT(AddDwarves(input_program), EqualsProto(expected_output_str));
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
