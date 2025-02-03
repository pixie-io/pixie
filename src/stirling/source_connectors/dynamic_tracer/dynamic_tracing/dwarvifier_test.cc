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

#include <memory>
#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dwarvifier.h"

constexpr std::string_view kBinaryPath = "src/stirling/obj_tools/testdata/cc/test_exe/test_exe";

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::px::testing::proto::EqualsProto;

constexpr std::string_view kEntryProbeIn = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: CPP
    probes {
      tracepoint: {
        symbol: "ABCSumMixed"
        type: ENTRY
      }
      args {
        id: "arg0"
        expr: "x"
      }
      args {
        id: "arg1"
        expr: "y"
      }
      args {
        id: "arg2"
        expr: "z_a"
      }
      args {
        id: "arg3"
        expr: "z_b"
      }
      args {
        id: "arg4"
        expr: "z_c"
      }
      args {
        id: "arg5"
        expr: "w"
      }
    }
  }
}
)";

constexpr std::string_view kEntryProbeOut = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
language: CPP
probes {
  tracepoint {
    symbol: "ABCSumMixed"
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
      name: "parm__"
      type: VOID_POINTER
      reg: SYSV_AMD64_ARGS_PTR
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: STRUCT_BLOB
      memory {
        base: "parm__"
        offset: 8
        size: 12
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: STRUCT_BLOB
      memory {
        base: "sp_"
        offset: 8
        size: 24
      }
    }
  }
  vars {
    scalar_var {
      name: "arg2"
      type: INT
      memory {
        base: "parm__"
        offset: 24
      }
    }
  }
  vars {
    scalar_var {
      name: "arg3"
      type: LONG
      memory {
        base: "parm__"
        offset: 32
      }
    }
  }
  vars {
    scalar_var {
      name: "arg4"
      type: INT
      memory {
        base: "parm__"
        offset: 40
      }
    }
  }
  vars {
    scalar_var {
      name: "arg5"
      type: STRUCT_BLOB
      memory {
        base: "sp_"
        size: 12
        offset: 32
      }
    }
  }
}
)";

constexpr std::string_view kReturnProbeIn = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: CPP
    probes {
      tracepoint: {
        symbol: "ABCSumMixed"
        type: RETURN
      }
      ret_vals {
        id: "retval0"
        expr: "$$0.a"
      }
    }
  }
}
)";

constexpr std::string_view kReturnProbeOut = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
language: CPP
probes {
  tracepoint {
    symbol: "ABCSumMixed"
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
      name: "rc_"
      type: VOID_POINTER
      reg: RC
    }
  }
  vars {
    scalar_var {
      name: "rc__"
      type: VOID_POINTER
      reg: RC_PTR
    }
  }
  vars {
    scalar_var {
      name: "retval0"
      type: LONG
      memory {
        base: "rc_"
      }
    }
  }
}
)";

constexpr std::string_view kNestedArgProbeIn = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: CPP
    probes {
      tracepoint: {
        symbol: "OuterStructFunc"
        type: ENTRY
      }
      args {
        id: "arg0"
        expr: "x.O1.M0.L1"
      }
    }
  }
}
)";

constexpr std::string_view kNestedArgProbeOut = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
language: CPP
probes {
  tracepoint {
    symbol: "OuterStructFunc"
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
      name: "parm__"
      type: VOID_POINTER
      reg: SYSV_AMD64_ARGS_PTR
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: INT
      memory {
        base: "sp_"
        offset: 20
      }
    }
  }
}
)";

constexpr std::string_view kActionProbeIn = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: CPP
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
        symbol: "ABCSumMixed"
        type: ENTRY
      }
      args {
        id: "arg0"
        expr: "x"
      }
      args {
        id: "arg1"
        expr: "y"
      }
      args {
        id: "arg2"
        expr: "y.a"
      }
      map_stash_actions {
        map_name: "my_stash"
        key: GOID
        value_variable_names: "arg0"
        value_variable_names: "arg1"
        cond {}
      }
      output_actions {
        output_name: "out_table"
        variable_names: "arg0"
        variable_names: "arg1"
        variable_names: "arg2"
      }
    }
    probes: {
      tracepoint: {
        symbol: "ABCSumMixed"
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
        variable_names: "arg0"
        variable_names: "arg1"
      }
    }
  }
}
)";

constexpr std::string_view kActionProbeOut = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
structs {
  name: "my_stash_value_t"
  fields {
    name: "arg0"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 4
        type: INT
        path: "/a"
      }
      entries {
        offset: 4
        size: 4
        type: INT
        path: "/b"
      }
      entries {
        offset: 8
        size: 4
        type: INT
        path: "/c"
      }
    }
  }
  fields {
    name: "arg1"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 8
        type: LONG
        path: "/a"
      }
      entries {
        offset: 8
        size: 8
        type: LONG
        path: "/b"
      }
      entries {
        offset: 16
        size: 8
        type: LONG
        path: "/c"
      }
    }
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
    name: "arg0"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 4
        type: INT
        path: "/a"
      }
      entries {
        offset: 4
        size: 4
        type: INT
        path: "/b"
      }
      entries {
        offset: 8
        size: 4
        type: INT
        path: "/c"
      }
    }
  }
  fields {
    name: "arg1"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 8
        type: LONG
        path: "/a"
      }
      entries {
        offset: 8
        size: 8
        type: LONG
        path: "/b"
      }
      entries {
        offset: 16
        size: 8
        type: LONG
        path: "/c"
      }
    }
  }
  fields {
    name: "arg2"
    type: LONG
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
    name: "arg0"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 4
        type: INT
        path: "/a"
      }
      entries {
        offset: 4
        size: 4
        type: INT
        path: "/b"
      }
      entries {
        offset: 8
        size: 4
        type: INT
        path: "/c"
      }
    }
  }
  fields {
    name: "arg1"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 8
        type: LONG
        path: "/a"
      }
      entries {
        offset: 8
        size: 8
        type: LONG
        path: "/b"
      }
      entries {
        offset: 16
        size: 8
        type: LONG
        path: "/c"
      }
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
  tracepoint {
    symbol: "ABCSumMixed"
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
      name: "parm__"
      type: VOID_POINTER
      reg: SYSV_AMD64_ARGS_PTR
    }
  }
  vars {
    scalar_var {
      name: "arg0"
      type: STRUCT_BLOB
      memory {
        base: "parm__"
        offset: 8
        size: 12
      }
    }
  }
  vars {
    scalar_var {
      name: "arg1"
      type: STRUCT_BLOB
      memory {
        base: "sp_"
        offset: 8
        size: 24
      }
    }
  }
  vars {
    scalar_var {
      name: "arg2"
      type: LONG
      memory {
        base: "sp_"
        offset: 8
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
  map_stash_actions {
    map_name: "my_stash"
    key_variable_name: "goid_"
    value_variable_name: "my_stash_value"
    cond {
    }
  }
  output_actions {
    perf_buffer_name: "out_table"
    data_buffer_array_name: "out_table_data_buffer_array"
    output_struct_name: "out_table_value_t"
    variable_names: "tgid_"
    variable_names: "tgid_start_time_"
    variable_names: "time_"
    variable_names: "arg0"
    variable_names: "arg1"
    variable_names: "arg2"
  }
}
probes {
  tracepoint {
    symbol: "ABCSumMixed"
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
      name: "rc_"
      type: VOID_POINTER
      reg: RC
    }
  }
  vars {
    scalar_var {
      name: "rc__"
      type: VOID_POINTER
      reg: RC_PTR
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
      type: STRUCT_BLOB
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
      type: STRUCT_BLOB
      member {
        struct_base: "my_stash_ptr"
        is_struct_base_pointer: true
        field: "arg1"
      }
    }
  }
  output_actions {
    perf_buffer_name: "out_table2"
    data_buffer_array_name: "out_table2_data_buffer_array"
    output_struct_name: "out_table2_value_t"
    variable_names: "tgid_"
    variable_names: "tgid_start_time_"
    variable_names: "time_"
    variable_names: "arg0"
    variable_names: "arg1"
  }
  map_delete_actions {
    map_name: "my_stash"
    key_variable_name: "goid_"
  }
}
language: CPP
arrays {
  name: "out_table_data_buffer_array"
  type {
    struct_type: "out_table_value_t"
  }
  capacity: 1
}
arrays {
  name: "out_table2_data_buffer_array"
  type {
    struct_type: "out_table2_value_t"
  }
  capacity: 1
}
)";

constexpr std::string_view kStructProbeIn = R"(
deployment_spec {
  path_list {
    paths: "$0"
  }
}
tracepoints {
  program {
    language: CPP
    outputs {
      name: "out_table"
      fields: "out"
    }
    probes {
      tracepoint: {
        symbol: "OuterStructFunc"
        type: ENTRY
      }
      args {
        id: "arg0"
        expr: "x"
      }
      output_actions {
        output_name: "out_table"
        variable_names: "arg0"
      }
    }
  }
}
)";

constexpr std::string_view kStructProbeOut = R"(
deployment_spec {
  path_list {
    paths: "$0"
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
    name: "out"
    type: STRUCT_BLOB
    blob_decoders {
      entries {
        size: 8
        type: LONG
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
        type: INT
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
        type: INT
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
    symbol: "OuterStructFunc"
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
      name: "parm__"
      type: VOID_POINTER
      reg: SYSV_AMD64_ARGS_PTR
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
  output_actions {
    perf_buffer_name: "out_table"
    data_buffer_array_name: "out_table_data_buffer_array"
    output_struct_name: "out_table_value_t"
    variable_names: "tgid_"
    variable_names: "tgid_start_time_"
    variable_names: "time_"
    variable_names: "arg0"
  }
}
language: CPP
arrays {
  name: "out_table_data_buffer_array"
  type {
    struct_type: "out_table_value_t"
  }
  capacity: 1
}
)";

struct DwarfInfoTestParam {
  std::string_view input;
  std::string_view expected_output;
};

class DwarfInfoTest : public ::testing::TestWithParam<DwarfInfoTestParam> {
 protected:
  DwarfInfoTest() : binary_path_(px::testing::BazelRunfilePath(kBinaryPath)) {}

  std::string binary_path_;
};

TEST_P(DwarfInfoTest, Transform) {
  using obj_tools::DwarfReader;
  using obj_tools::ElfReader;

  DwarfInfoTestParam p = GetParam();

  std::string input_str = absl::Substitute(p.input, binary_path_);
  ir::logical::TracepointDeployment input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(std::string(input_str), &input_program));

  std::string expected_output_str = absl::Substitute(p.expected_output, binary_path_);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::CreateIndexingAll(binary_path_));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(binary_path_));
  ASSERT_OK_AND_ASSIGN(
      ir::physical::Program physical_program,
      GeneratePhysicalProgram(input_program, dwarf_reader.get(), elf_reader.get()));

// Check for `bazel coverage` so we can bypass the final checks.
// Note that we still get accurate coverage metrics, because this only skips the final check.
// Ideally, we'd get bazel to deterministically build test_go_binary,
// but it's not easy to tell bazel to use a different config for just one target.
#ifdef PL_COVERAGE
  LOG(INFO) << "Whoa...`bazel coverage` is messaging with test_go_binary. Shame on you bazel. "
               "Skipping final checks.";
  return;
#else
  ASSERT_THAT(physical_program, EqualsProto(expected_output_str));
#endif
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
}  // namespace px
