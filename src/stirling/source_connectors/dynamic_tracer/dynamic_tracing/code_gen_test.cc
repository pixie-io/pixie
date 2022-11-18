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

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/code_gen.h"

#include <google/protobuf/text_format.h>

#include "src/common/testing/testing.h"

constexpr std::string_view kBinaryPath = "src/stirling/obj_tools/testdata/go/test_go_1_16_binary";

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::px::stirling::dynamic_tracing::ir::physical::Field;
using ::px::stirling::dynamic_tracing::ir::physical::MapStashAction;
using ::px::stirling::dynamic_tracing::ir::physical::PerfBufferOutputAction;
using ::px::stirling::dynamic_tracing::ir::physical::Register;
using ::px::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::px::stirling::dynamic_tracing::ir::physical::Struct;
using ::px::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::px::stirling::dynamic_tracing::ir::shared::BPFHelper;
using ::px::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::StrEq;

TEST(GenStructTest, Output) {
  Struct st;
  st.set_name("socket_data_event_t");

  Field* field = nullptr;

  field = st.add_fields();
  field->set_name("i32");
  field->set_type(ScalarType::INT32);

  field = st.add_fields();
  field->set_name("i64");
  field->set_type(ScalarType::INT64);

  field = st.add_fields();
  field->set_name("double_val");
  field->set_type(ScalarType::DOUBLE);

  field = st.add_fields();
  field->set_name("msg");
  field->set_type(ScalarType::VOID_POINTER);

  ASSERT_OK_AND_THAT(GenStruct(st, /*indent_size*/ 4),
                     ElementsAre("struct socket_data_event_t {", "    int32_t i32;",
                                 "    int64_t i64;", "    double double_val;", "    void* msg;",
                                 "} __attribute__((packed, aligned(1)));"));
}

TEST(GenVariableTest, Register) {
  ScalarVariable var;

  var.set_name("var");

  var.set_type(ScalarType::VOID_POINTER);
  var.set_reg(Register::SP);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("void* var = (void*)PT_REGS_SP(ctx);"));

  var.set_type(ScalarType::INT);
  var.set_reg(Register::RC);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("int var = (int)PT_REGS_RC(ctx);"));

  var.set_type(ScalarType::VOID_POINTER);
  var.set_reg(Register::RC);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("void* var = (void*)PT_REGS_RC(ctx);"));

  var.set_type(ScalarType::INT64);
  var.set_reg(Register::RC_PTR);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("uint64_t rc___[2];"
                                                         "rc___[0] = ctx->ax;"
                                                         "rc___[1] = ctx->dx;"
                                                         "void* var = &rc___;"));

  var.set_type(ScalarType::INT64);
  var.set_reg(Register::RDX);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("int64_t var = (int64_t)ctx->dx;"));
}

TEST(GenVariableTest, MemoryVariable) {
  ScalarVariable var;

  var.set_name("var");
  var.set_type(ScalarType::INT32);

  auto* mem_var = var.mutable_memory();

  mem_var->set_base("sp");
  mem_var->set_offset(123);

  ASSERT_OK_AND_THAT(
      GenScalarVariable(var),
      ElementsAre("int32_t var;", "bpf_probe_read(&var, sizeof(int32_t), sp + 123);"));
}

TEST(GenVariableTest, Builtin) {
  ScalarVariable var;

  var.set_name("var");
  var.set_type(ScalarType::VOID_POINTER);

  var.set_builtin(BPFHelper::GOID);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("void* var = pl_goid();"));

  var.set_builtin(BPFHelper::TGID);

  ASSERT_OK_AND_THAT(GenScalarVariable(var),
                     ElementsAre("void* var = bpf_get_current_pid_tgid() >> 32;"));

  var.set_builtin(BPFHelper::TGID_PID);

  ASSERT_OK_AND_THAT(GenScalarVariable(var),
                     ElementsAre("void* var = bpf_get_current_pid_tgid();"));

  var.set_builtin(BPFHelper::KTIME);
  var.set_type(ScalarType::UINT64);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("uint64_t var = bpf_ktime_get_ns();"));

  var.set_builtin(BPFHelper::TGID_START_TIME);
  var.set_type(ScalarType::UINT64);
  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("uint64_t var = pl_tgid_start_time();"));
}

TEST(GenStructVariableTest, Variables) {
  Struct st;

  st.set_name("socket_data_event_t");

  Field* field = nullptr;

  field = st.add_fields();
  field->set_name("i32");
  field->set_type(ScalarType::INT32);

  field = st.add_fields();
  field->set_name("i64");
  field->set_type(ScalarType::INT64);

  StructVariable st_var;

  st_var.set_name("st_var");
  st_var.set_type("socket_data_event_t");

  auto* fa = st_var.add_field_assignments();
  fa->set_field_name("i32");
  fa->set_variable_name("foo");

  fa = st_var.add_field_assignments();
  fa->set_field_name("i64");
  fa->set_variable_name("bar");

  ASSERT_OK_AND_THAT(GenStructVariable(st_var),
                     ElementsAre("struct socket_data_event_t st_var = {};", "st_var.i32 = foo;",
                                 "st_var.i64 = bar;"));

  st_var.set_is_pointer(true);
  ASSERT_OK_AND_THAT(GenStructVariable(st_var),
                     ElementsAre("struct socket_data_event_t st_var = {};", "st_var->i32 = foo;",
                                 "st_var->i64 = bar;"));
}

TEST(GenMapStashActionTest, StashMap) {
  MapStashAction action;

  action.set_map_name("test");
  action.set_key_variable_name("foo");
  action.set_value_variable_name("bar");

  action.mutable_cond()->set_op(ir::shared::Condition::EQUAL);
  action.mutable_cond()->add_vars("foo");
  action.mutable_cond()->add_vars("bar");

  ASSERT_OK_AND_THAT(GenMapStashAction(action),
                     ElementsAre("if (foo == bar) {", "test.update(&foo, &bar);", "}"));
}

TEST(GenProgramTest, SpecsAndCode) {
  const std::string program_protobuf = R"proto(
                                       deployment_spec {
                                         path_list {
                                           paths: "target_binary_path"
                                         }
                                       }
                                       language: GOLANG
                                       structs {
                                         name: "socket_data_event_t"
                                         fields {
                                           name: "i32"
                                           type: INT32
                                         }
                                       }
                                       structs {
                                         name: "value_t"
                                         fields {
                                           name: "i32"
                                           type: INT32
                                         }
                                       }
                                       maps {
                                         name: "map"
                                         key_type { scalar: UINT32 }
                                         value_type { struct_type: "value_t" }
                                       }
                                       arrays {
                                         name: "array"
                                         type { struct_type: "socket_data_event_t" }
                                         capacity: 1
                                       }
                                       outputs {
                                          name: "data_events"
                                          struct_type: "socket_data_event_t"
                                       }
                                       outputs {
                                          name: "data_events2"
                                          struct_type: "socket_data_event_t"
                                       }
                                       probes {
                                         name: "probe_entry"
                                         tracepoint {
                                           symbol: "target_symbol"
                                           type: ENTRY
                                         }
                                         vars {
                                           scalar_var {
                                             name: "key"
                                             type: UINT32
                                             builtin: TGID
                                           }
                                         }
                                         vars {
                                           scalar_var {
                                             name: "var"
                                             type: INT32
                                             reg: SP
                                           }
                                         }
                                         vars {
                                           scalar_var {
                                             name: "struct_blob"
                                             type: STRUCT_BLOB
                                             memory {
                                               base: "var"
                                               offset: 16
                                               size: 8
                                               op: DEFINE_ONLY
                                             }
                                           }
                                         }
                                         vars {
                                           struct_var {
                                             name: "st_var"
                                             type: "socket_data_event_t"
                                             field_assignments {
                                               field_name: "i32"
                                               variable_name: "var"
                                             }
                                           }
                                         }
                                         vars {
                                           map_var {
                                             name: "map_var"
                                             type: "value_t"
                                             map_name: "map"
                                             key_variable_name: "key"
                                           }
                                         }
                                         vars {
                                           scalar_var {
                                             name: "index"
                                             type: INT32
                                             constant: "0"
                                           }
                                         }
                                         vars {
                                           map_var {
                                             name: "array_var"
                                             type: "socket_data_event_t"
                                             map_name: "array"
                                             key_variable_name: "index"
                                           }
                                         }
                                         cond_blocks {
                                           cond {
                                             op: EQUAL
                                             vars: "key"
                                             vars: "var"
                                           }
                                           vars {
                                             scalar_var {
                                               name: "struct_blob"
                                               type: STRUCT_BLOB
                                               memory {
                                                 base: "var"
                                                 offset: 16
                                                 size: 8
                                                 op: ASSIGN_ONLY
                                               }
                                             }
                                           }
                                           vars {
                                             scalar_var {
                                               name: "inner_var"
                                               type: INT32
                                               reg: SP
                                             }
                                           }
                                           vars {
                                             struct_var {
                                               name: "st_var"
                                               type: "socket_data_event_t"
                                               field_assignments {
                                                 field_name: "i32"
                                                 variable_name: "inner_var"
                                               }
                                             }
                                           }
                                           return_value: "0"
                                         }
                                         map_stash_actions {
                                           map_name: "test"
                                           key_variable_name: "key"
                                           value_variable_name: "var"
                                         }
                                         output_actions {
                                           perf_buffer_name: "data_events"
                                           data_buffer_array_name: "data_events_value_array"
                                           output_struct_name: "socket_data_event_t"
                                           variable_names: "inner_var"
                                         }
                                         printks { scalar: "var" }
                                       }
                                       probes {
                                         name: "probe_return"
                                         tracepoint {
                                           symbol: "target_symbol"
                                           type: RETURN
                                         }
                                         vars {
                                           scalar_var {
                                             name: "key"
                                             type: UINT32
                                             builtin: TGID
                                           }
                                         }
                                         vars {
                                           scalar_var {
                                             name: "retval"
                                             type: INT
                                             reg: RC
                                           }
                                         }
                                         map_delete_actions {
                                           map_name: "test"
                                           key_variable_name: "key"
                                         }
                                       }
                                       )proto";

  ir::physical::Program program;

  ASSERT_TRUE(TextFormat::ParseFromString(program_protobuf, &program));
  program.mutable_deployment_spec()->mutable_path_list()->add_paths(
      px::testing::BazelRunfilePath(kBinaryPath));

  ASSERT_OK_AND_ASSIGN(const std::string bcc_code, GenBCCProgram(program));

  const std::vector<std::string> expected_code_lines = {
      "#include <linux/sched.h>",
      "#define __inline inline __attribute__((__always_inline__))",
      "static __inline uint64_t pl_nsec_to_clock_t(uint64_t x) {",
      "return div_u64(x, NSEC_PER_SEC / USER_HZ);",
      "}",
      "static __inline uint64_t pl_tgid_start_time() {",
      "struct task_struct* task_group_leader = ((struct "
      "task_struct*)bpf_get_current_task())->group_leader;",
      "#if LINUX_VERSION_CODE >= 328960",
      "return pl_nsec_to_clock_t(task_group_leader->start_boottime);",
      "#else",
      "return pl_nsec_to_clock_t(task_group_leader->real_start_time);",
      "#endif",
      "}",
      "struct blob32 {",
      "  uint64_t len;",
      "  uint8_t buf[32-9];",
      "  uint8_t truncated;",
      "};",
      "struct blob64 {",
      "  uint64_t len;",
      "  uint8_t buf[64-9];",
      "  uint8_t truncated;",
      "};",
      "struct struct_blob64 {",
      "  uint64_t len;",
      "  int8_t decoder_idx;",
      "  uint8_t buf[64-10];",
      "  uint8_t truncated;",
      "};",
      "struct socket_data_event_t {",
      "  int32_t i32;",
      "} __attribute__((packed, aligned(1)));",
      "struct value_t {",
      "  int32_t i32;",
      "} __attribute__((packed, aligned(1)));",
      "BPF_HASH(map, uint32_t, struct value_t);",
      "BPF_PERCPU_ARRAY(array, struct socket_data_event_t, 1);",
      "static __inline int64_t pl_goid() {",
      "uint64_t current_pid_tgid = bpf_get_current_pid_tgid();",
      "const struct pid_goid_map_value_t* goid_ptr = pid_goid_map.lookup(&current_pid_tgid);",
      "return (goid_ptr == NULL) ? -1 : goid_ptr->goid;",
      "}",
      "BPF_PERF_OUTPUT(data_events);",
      "BPF_PERF_OUTPUT(data_events2);",
      "int probe_entry(struct pt_regs* ctx) {",
      "uint32_t key = bpf_get_current_pid_tgid() >> 32;",
      "int32_t var = (int32_t)PT_REGS_SP(ctx);",
      "struct struct_blob64 struct_blob = {};",
      "struct socket_data_event_t st_var = {};",
      "st_var.i32 = var;",
      "struct value_t* map_var = map.lookup(&key);",
      "int32_t index = 0;",
      "struct socket_data_event_t* array_var = array.lookup(&index);",
      "if (key == var) {",
      "struct_blob.len = 8;",
      "struct_blob.decoder_idx = 0;",
      "bpf_probe_read(&struct_blob.buf, 8, var + 16);",
      "int32_t inner_var = (int32_t)PT_REGS_SP(ctx);",
      "struct socket_data_event_t st_var = {};",
      "st_var.i32 = inner_var;",
      "return 0;",
      "}",
      "test.update(&key, &var);",
      "uint32_t data_events_value_idx = 0;",
      "struct socket_data_event_t* data_events_value = "
      "data_events_value_array.lookup(&data_events_value_idx);",
      "if (data_events_value == NULL) { return 0; }",
      "data_events_value->i32 = inner_var;",
      "data_events.perf_submit(ctx, data_events_value, sizeof(*data_events_value));",
      R"(bpf_trace_printk("var: %d\n", var);)",
      "return 0;",
      "}",
      "int probe_return(struct pt_regs* ctx) {",
      "uint32_t key = bpf_get_current_pid_tgid() >> 32;",
      "int retval = (int)PT_REGS_RC(ctx);",
      "test.delete(&key);",
      "return 0;",
      "}"};

  std::vector<std::string> bcc_code_lines = absl::StrSplit(bcc_code, "\n");
  EXPECT_THAT(bcc_code_lines, ElementsAreArray(expected_code_lines));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
