#include "src/stirling/dynamic_tracing/code_gen.h"

#include <google/protobuf/text_format.h>

#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::dynamic_tracing::ir::physical::MapStashAction;
using ::pl::stirling::dynamic_tracing::ir::physical::PerfBufferOutputAction;
using ::pl::stirling::dynamic_tracing::ir::physical::Register;
using ::pl::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::Struct;
using ::pl::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::pl::stirling::dynamic_tracing::ir::shared::BPFHelper;
using ::pl::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::StrEq;

TEST(GenStructTest, Output) {
  Struct st;
  st.set_name("socket_data_event_t");

  Struct::Field* field = nullptr;

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

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::GOLANG),
                     ElementsAre("void* var = (void*)PT_REGS_SP(ctx);"));

  var.set_type(ScalarType::INT);
  var.set_reg(Register::RC);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::CPP),
                     ElementsAre("int var = (int)PT_REGS_RC(ctx);"));

  var.set_type(ScalarType::VOID_POINTER);
  var.set_reg(Register::RC);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::CPP),
                     ElementsAre("void* var = (void*)PT_REGS_RC(ctx);"));

  var.set_type(ScalarType::INT64);
  var.set_reg(Register::RC_PTR);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::CPP),
                     ElementsAre("uint64_t rc___[2];"
                                 "rc___[0] = PT_REGS_RC(ctx);"
                                 "rc___[1] = PT_REGS_PARM3(ctx);"
                                 "void* var = &rc___;"));

  var.set_type(ScalarType::INT64);
  var.set_reg(Register::PARM3);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::CPP),
                     ElementsAre("int64_t var = (int64_t)PT_REGS_PARM3(ctx);"));
}

TEST(GenVariableTest, MemoryVariable) {
  ScalarVariable var;

  var.set_name("var");
  var.set_type(ScalarType::INT32);

  auto* mem_var = var.mutable_memory();

  mem_var->set_base("sp");
  mem_var->set_offset(123);

  ASSERT_OK_AND_THAT(
      GenScalarVariable(var, ir::shared::Language::GOLANG),
      ElementsAre("int32_t var;", "bpf_probe_read(&var, sizeof(int32_t), sp + 123);"));
}

TEST(GenVariableTest, Builtin) {
  ScalarVariable var;

  var.set_name("var");
  var.set_type(ScalarType::VOID_POINTER);

  var.set_builtin(BPFHelper::GOID);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::GOLANG),
                     ElementsAre("void* var = pl_goid();"));

  var.set_builtin(BPFHelper::TGID);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::GOLANG),
                     ElementsAre("void* var = bpf_get_current_pid_tgid() >> 32;"));

  var.set_builtin(BPFHelper::TGID_PID);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::GOLANG),
                     ElementsAre("void* var = bpf_get_current_pid_tgid();"));

  var.set_builtin(BPFHelper::KTIME);
  var.set_type(ScalarType::UINT64);

  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::GOLANG),
                     ElementsAre("uint64_t var = bpf_ktime_get_ns();"));

  var.set_builtin(BPFHelper::TGID_START_TIME);
  var.set_type(ScalarType::UINT64);
  ASSERT_OK_AND_THAT(GenScalarVariable(var, ir::shared::Language::GOLANG),
                     ElementsAre("uint64_t var = pl_tgid_start_time();"));
}

TEST(GenStructVariableTest, Variables) {
  Struct st;

  st.set_name("socket_data_event_t");

  Struct::Field* field = nullptr;

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

  ASSERT_OK_AND_THAT(GenStructVariable(st, st_var),
                     ElementsAre("struct socket_data_event_t st_var = {};", "st_var.i32 = foo;",
                                 "st_var.i64 = bar;"));
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
                                         path: "target_binary_path"
                                       }
                                       language: GOLANG
                                       structs {
                                         name: "socket_data_event_t"
                                         fields {
                                           name: "i32"
                                           type: INT32
                                         }
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
                                           struct_var {
                                             name: "st_var"
                                             type: "socket_data_event_t"
                                             field_assignments {
                                               field_name: "i32"
                                               variable_name: "var"
                                             }
                                           }
                                         }
                                         map_stash_actions {
                                           map_name: "test"
                                           key_variable_name: "key"
                                           value_variable_name: "var"
                                         }
                                         output_actions {
                                           perf_buffer_name: "data_events"
                                           variable_name: "st_var"
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
  program.mutable_deployment_spec()->set_path(pl::testing::BazelBinTestFilePath(kBinaryPath));

  ASSERT_OK_AND_ASSIGN(const std::string bcc_code, GenProgram(program));

  const std::vector<std::string> expected_code_lines = {
      "#include <linux/ptrace.h>",
      "#include <linux/sched.h>",
      "#ifndef __inline",
      "#ifdef SUPPORT_BPF2BPF_CALL",
      "#define __inline",
      "#else",
      "#define __inline inline __attribute__((__always_inline__))",
      "#endif",
      "#endif",
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
      "#define MAX_STR_LEN (32-sizeof(int64_t)-1)",
      "#define MAX_STR_MASK (32-1)",
      "struct string {",
      "  uint64_t len;",
      "  char buf[MAX_STR_LEN];",
      "  // To keep 4.14 kernel verifier happy, we copy an extra byte.",
      "  // Keep a dummy character to absorb this garbage.",
      "  char dummy;",
      "};",
      "#define MAX_BYTE_ARRAY_LEN (64-sizeof(int64_t)-1)",
      "#define MAX_BYTE_ARRAY_MASK (64-1)",
      "struct byte_array {",
      "  uint64_t len;",
      "  uint8_t buf[MAX_BYTE_ARRAY_LEN];",
      "  // To keep 4.14 kernel verifier happy, we copy an extra byte.",
      "  // Keep a dummy character to absorb this garbage.",
      "  char dummy;",
      "};",
      "struct socket_data_event_t {",
      "  int32_t i32;",
      "} __attribute__((packed, aligned(1)));",
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
      "struct socket_data_event_t st_var = {};",
      "st_var.i32 = var;",
      "test.update(&key, &var);",
      "data_events.perf_submit(ctx, &st_var, sizeof(st_var));",
      R"(bpf_trace_printk("var: %d\n", var);)",
      "return 0;",
      "}",
      "int probe_return(struct pt_regs* ctx) {",
      "uint32_t key = bpf_get_current_pid_tgid() >> 32;",
      "int retval = (int)PT_REGS_RC(ctx);",
      "test.delete(&key);",
      "return 0;",
      "}"};

  std::string expected_code = absl::StrJoin(expected_code_lines, "\n");
  EXPECT_EQ(bcc_code, expected_code);
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
