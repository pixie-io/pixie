#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dwarvifier.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::testing::proto::EqualsProto;

constexpr std::string_view kEntryProbeIn = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
tracepoints {
  program {
    language: GOLANG
    probes {
      tracepoint: {
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
  }
}
)";

constexpr std::string_view kEntryProbeOut = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
language: GOLANG
probes {
  tracepoint {
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp_"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid_"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid_"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time_"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "time_"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid_"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      memory {
        base: "sp_"
        offset: 8
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: INT
      memory {
        base: "sp_"
        offset: 24
      }
    }
  }
  vars {
    scalar_var {
      name: "arg2"
      type: INT
      memory {
        base: "sp_"
        offset: 32
      }
    }
  }
  vars {
    scalar_var {
      name: "arg3"
      type: BOOL
      memory {
        base: "sp_"
        offset: 16
      }
    }
  }
  vars {
    scalar_var {
      name: "arg4"
      type: BOOL
      memory {
        base: "sp_"
        offset: 17
      }
    }
  }
  vars {
    scalar_var {
      name: "arg5"
      type: BOOL
      memory {
        base: "sp_"
        offset: 20
      }
    }
  }
}
)";

constexpr std::string_view kReturnProbeIn = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
tracepoints {
  program {
    language: GOLANG
    probes {
      tracepoint: {
        symbol: "main.MixedArgTypes"
        type: RETURN
      }
      ret_vals {
        id: "retval0"
        expr: "$$6"
      }
      ret_vals {
        id: "retval1"
        expr: "$$7.B1"
      }
    }
  }
}
)";

constexpr std::string_view kReturnProbeOut = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
language: GOLANG
probes {
  tracepoint {
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  vars {
    scalar_var {
      name: "sp_"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid_"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid_"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time_"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "time_"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid_"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "retval0"
      type: INT
      memory {
        base: "sp_"
        offset: 48
      }
    }
  }
  vars {
    scalar_var {
      name: "retval1"
      type: BOOL
      memory {
        base: "sp_"
        offset: 57
      }
    }
  }
}
)";

constexpr std::string_view kNestedArgProbeIn = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
tracepoints {
  program {
    language: GOLANG
    probes {
      tracepoint: {
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
  }
}
)";

constexpr std::string_view kNestedArgProbeOut = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
language: GOLANG
probes {
  tracepoint {
    symbol: "main.PointerWrapperWrapperWrapperFunc"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp_"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid_"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid_"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time_"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "time_"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid_"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0_D_Ptr_X_"
      type: VOID_POINTER
      memory {
        base: "sp_"
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
        base: "sp_"
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
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
tracepoints {
  program {
    language: GOLANG
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
      tracepoint: {
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
      tracepoint: {
        symbol: "main.MixedArgTypes"
        type: RETURN
      }
      map_vals {
        map_name: "my_stash"
        key: GOID
        value_ids: "arg0"
        value_ids: "arg1"
      }
      map_delete_actions {
        map_name: "my_stash"
        key: GOID
      }
      output_actions {
        output_name: "out_table2"
        variable_name: "arg0"
        variable_name: "arg1"
      }
    }
  }
}
)";

constexpr std::string_view kActionProbeOut = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
language: GOLANG
structs {
  name: "my_stash_value_t"
  fields {
    name: "arg0"
    type: INT
  }
  fields {
    name: "arg1"
    type: BOOL
  }
}
structs {
  name: "out_table_value_t"
  fields {
    name: "tgid_"
    type: INT32
  }
  fields {
    name: "tgid_start_time_"
    type: UINT64
  }
  fields {
    name: "time_"
    type: UINT64
  }
  fields {
    name: "goid_"
    type: INT64
  }
  fields {
    name: "arg0"
    type: INT
  }
  fields {
    name: "arg1"
    type: BOOL
  }
  fields {
    name: "arg2"
    type: BOOL
  }
}
structs {
  name: "out_table2_value_t"
  fields {
    name: "tgid_"
    type: INT32
  }
  fields {
    name: "tgid_start_time_"
    type: UINT64
  }
  fields {
    name: "time_"
    type: UINT64
  }
  fields {
    name: "goid_"
    type: INT64
  }
  fields {
    name: "arg0"
    type: INT
  }
  fields {
    name: "arg1"
    type: BOOL
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
  tracepoint {
    symbol: "main.MixedArgTypes"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp_"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid_"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid_"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time_"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "time_"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid_"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      memory {
        base: "sp_"
        offset: 8
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: BOOL
      memory {
        base: "sp_"
        offset: 16
      }
    }
  }
  vars {
    scalar_var {
      name: "arg2"
      type: BOOL
      memory {
        base: "sp_"
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
        field_name: "tgid_"
        variable_name: "tgid_"
      }
      field_assignments {
        field_name: "tgid_start_time_"
        variable_name: "tgid_start_time_"
      }
      field_assignments {
        field_name: "time_"
        variable_name: "time_"
      }
      field_assignments {
        field_name: "goid_"
        variable_name: "goid_"
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
    key_variable_name: "goid_"
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
  tracepoint {
    symbol: "main.MixedArgTypes"
    type: RETURN
  }
  vars {
    scalar_var {
      name: "sp_"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid_"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid_"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time_"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "time_"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid_"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    map_var {
      name: "my_stash_ptr"
      type: "my_stash_value_t"
      map_name: "my_stash"
      key_variable_name: "goid_"
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      member {
        struct_base: "my_stash_ptr"
        is_struct_base_pointer: true
        field: "arg0"
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: BOOL
      member {
        struct_base: "my_stash_ptr"
        is_struct_base_pointer: true
        field: "arg1"
      }
    }
  }
  vars {
    struct_var {
      name: "out_table2_value"
      type: "out_table2_value_t"
      field_assignments {
        field_name: "tgid_"
        variable_name: "tgid_"
      }
      field_assignments {
        field_name: "tgid_start_time_"
        variable_name: "tgid_start_time_"
      }
      field_assignments {
        field_name: "time_"
        variable_name: "time_"
      }
      field_assignments {
        field_name: "goid_"
        variable_name: "goid_"
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
  map_delete_actions {
    map_name: "my_stash"
    key_variable_name: "goid_"
  }
  output_actions {
    perf_buffer_name: "out_table2"
    variable_name: "out_table2_value"
  }
}
)";

constexpr std::string_view kStructProbeIn = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "out_table"
      fields: "out"
    }
    probes {
      tracepoint: {
        symbol: "main.OuterStructFunc"
        type: ENTRY
      }
      args {
        id: "arg0"
        expr: "x"
      }
      output_actions {
        output_name: "out_table"
        variable_name: "arg0"
      }
    }
  }
}
)";

constexpr std::string_view kStructProbeOut = R"(
deployment_spec {
  path: "$0"
  debug_symbols_path: "$0"
}
language: GOLANG
structs {
  name: "out_table_value_t"
  fields {
    name: "tgid_"
    type: INT32
  }
  fields {
    name: "tgid_start_time_"
    type: UINT64
  }
  fields {
    name: "time_"
    type: UINT64
  }
  fields {
    name: "goid_"
    type: INT64
  }
  fields {
    name: "out"
    type: STRUCT_BLOB
    blob_decoder {
      entries {
        size: 8
        type: INT64
        path: "/O0"
      }
      entries {
        offset: 8
        size: 1
        type: BOOL
        path: "/O1/M0/L0"
      }
      entries {
        offset: 12
        size: 4
        type: INT32
        path: "/O1/M0/L1"
      }
      entries {
        offset: 16
        size: 8
        type: VOID_POINTER
        path: "/O1/M0/L2"
      }
      entries {
        offset: 24
        size: 1
        type: BOOL
        path: "/O1/M1"
      }
      entries {
        offset: 32
        size: 1
        type: BOOL
        path: "/O1/M2/L0"
      }
      entries {
        offset: 36
        size: 4
        type: INT32
        path: "/O1/M2/L1"
      }
      entries {
        offset: 40
        size: 8
        type: VOID_POINTER
        path: "/O1/M2/L2"
      }
    }
  }
}
outputs {
  name: "out_table"
  fields: "out"
  struct_type: "out_table_value_t"
}
probes {
  tracepoint {
    symbol: "main.OuterStructFunc"
    type: ENTRY
  }
  vars {
    scalar_var {
      name: "sp_"
      type: VOID_POINTER
      reg: SP
    }
  }
  vars {
    scalar_var {
      name: "tgid_"
      type: INT32
      builtin: TGID
    }
  }
  vars {
    scalar_var {
      name: "tgid_pid_"
      type: UINT64
      builtin: TGID_PID
    }
  }
  vars {
    scalar_var {
      name: "tgid_start_time_"
      type: UINT64
      builtin: TGID_START_TIME
    }
  }
  vars {
    scalar_var {
      name: "time_"
      type: UINT64
      builtin: KTIME
    }
  }
  vars {
    scalar_var {
      name: "goid_"
      type: INT64
      builtin: GOID
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: STRUCT_BLOB
      memory {
        base: "sp_"
        offset: 8
        size: 48
      }
    }
  }
  vars {
    struct_var {
      name: "out_table_value"
      type: "out_table_value_t"
      field_assignments {
        field_name: "tgid_"
        variable_name: "tgid_"
      }
      field_assignments {
        field_name: "tgid_start_time_"
        variable_name: "tgid_start_time_"
      }
      field_assignments {
        field_name: "time_"
        variable_name: "time_"
      }
      field_assignments {
        field_name: "goid_"
        variable_name: "goid_"
      }
      field_assignments {
        field_name: "out"
        variable_name: "arg0"
      }
    }
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
  DwarfInfoTest() : binary_path_(pl::testing::BazelBinTestFilePath(kBinaryPath)) {}

  std::string binary_path_;
};

TEST_P(DwarfInfoTest, Transform) {
  DwarfInfoTestParam p = GetParam();

  std::string input_str = absl::Substitute(p.input, binary_path_);
  ir::logical::TracepointDeployment input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(std::string(input_str), &input_program));

  std::string expected_output_str = absl::Substitute(p.expected_output, binary_path_);
  ASSERT_OK_AND_THAT(GeneratePhysicalProgram(input_program), EqualsProto(expected_output_str));
}

INSTANTIATE_TEST_SUITE_P(DwarfInfoTestSuite, DwarfInfoTest,
                         ::testing::Values(DwarfInfoTestParam{kEntryProbeIn, kEntryProbeOut},
                                           DwarfInfoTestParam{kReturnProbeIn, kReturnProbeOut},
                                           DwarfInfoTestParam{kNestedArgProbeIn,
                                                              kNestedArgProbeOut},
                                           DwarfInfoTestParam{kActionProbeIn, kActionProbeOut},
                                           DwarfInfoTestParam{kStructProbeIn, kStructProbeOut}));

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
