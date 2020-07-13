#include "src/stirling/dynamic_tracing/code_gen.h"

#include <google/protobuf/text_format.h>

#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::stirling::dynamic_tracing::ir::physical::MapStashAction;
using ::pl::stirling::dynamic_tracing::ir::physical::OutputAction;
using ::pl::stirling::dynamic_tracing::ir::physical::Probe;
using ::pl::stirling::dynamic_tracing::ir::physical::Register;
using ::pl::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::Struct;
using ::pl::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::pl::stirling::dynamic_tracing::ir::shared::BPFHelper;
using ::pl::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::pl::stirling::dynamic_tracing::ir::shared::VariableType;
using ::pl::testing::proto::EqualsProto;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::EndsWith;
using ::testing::Field;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(GenStructTest, Output) {
  Struct st;
  st.set_name("socket_data_event_t");

  Struct::Field* field = nullptr;

  field = st.add_fields();
  field->set_name("i32");
  field->mutable_type()->set_scalar(ScalarType::INT32);

  field = st.add_fields();
  field->set_name("i64");
  field->mutable_type()->set_scalar(ScalarType::INT64);

  field = st.add_fields();
  field->set_name("double_val");
  field->mutable_type()->set_scalar(ScalarType::DOUBLE);

  field = st.add_fields();
  field->set_name("msg");
  field->mutable_type()->set_scalar(ScalarType::VOID_POINTER);

  field = st.add_fields();
  field->set_name("attr");
  field->mutable_type()->set_struct_type("attr_t");

  ASSERT_OK_AND_THAT(
      GenStruct(st, /*indent_size*/ 4),
      ElementsAre("struct socket_data_event_t {", "    int32_t i32;", "    int64_t i64;",
                  "    double double_val;", "    void* msg;", "    struct attr_t attr;", "};"));
}

TEST(GenVariableTest, Register) {
  ScalarVariable var;

  var.set_name("var");
  var.set_type(ScalarType::VOID_POINTER);
  var.set_reg(Register::SP);

  ASSERT_OK_AND_THAT(GenScalarVariable(var), ElementsAre("void* var = (void*)PT_REGS_SP(ctx);"));
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

  Struct::Field* field = nullptr;

  field = st.add_fields();
  field->set_name("i32");
  field->mutable_type()->set_scalar(ScalarType::INT32);

  field = st.add_fields();
  field->set_name("i64");
  field->mutable_type()->set_scalar(ScalarType::INT64);

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

TEST(GenOutputActionTest, Variables) {
  OutputAction action;

  action.set_perf_buffer_name("test");
  action.set_variable_name("foo");

  EXPECT_THAT(GenOutputAction(action), StrEq("test.perf_submit(ctx, &foo, sizeof(foo));"));
}

TEST(GenProgramTest, SpecsAndCode) {
  const std::string program_protobuf = R"proto(
                                       binary_path: "target_binary_path"
                                       structs {
                                         name: "socket_data_event_t"
                                         fields {
                                           name: "i32"
                                           type { scalar: INT32 }
                                         }
                                       }
                                       outputs {
                                          name: "data_events"
                                          type { struct_type: "socket_data_event_t" }
                                       }
                                       outputs {
                                          name: "data_events2"
                                          type { struct_type: "socket_data_event_t" }
                                       }
                                       probes {
                                         name: "probe_entry"
                                         trace_point {
                                           symbol: "target_symbol"
                                           type: ENTRY
                                         }
                                         vars {
                                           name: "key"
                                           type: UINT32
                                           builtin: TGID
                                         }
                                         vars {
                                           name: "var"
                                           type: INT32
                                           reg: SP
                                         }
                                         st_vars {
                                           name: "st_var"
                                           type: "socket_data_event_t"
                                           field_assignments {
                                             field_name: "i32"
                                             variable_name: "var"
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
                                       )proto";

  ir::physical::Program program;

  ASSERT_TRUE(TextFormat::ParseFromString(program_protobuf, &program));

  ASSERT_OK_AND_ASSIGN(const BCCProgram bcc_program, GenProgram(program));

  ASSERT_THAT(bcc_program.uprobes, SizeIs(1));

  const auto& spec = bcc_program.uprobes[0];

  EXPECT_THAT(spec, Field(&UProbeSpec::binary_path, "target_binary_path"));
  EXPECT_THAT(spec, Field(&UProbeSpec::symbol, "target_symbol"));
  EXPECT_THAT(spec, Field(&UProbeSpec::attach_type, bpf_tools::BPFProbeAttachType::kEntry));
  EXPECT_THAT(spec, Field(&UProbeSpec::probe_fn, "probe_entry"));

  ASSERT_THAT(bcc_program.perf_buffer_specs, SizeIs(2));

  const auto& perf_buffer_name = bcc_program.perf_buffer_specs[0].name;
  const auto& perf_buffer_output = bcc_program.perf_buffer_specs[0].output;

  EXPECT_THAT(perf_buffer_name, "data_events");
  EXPECT_THAT(perf_buffer_output, EqualsProto(
                                      R"proto(
                                      name: "socket_data_event_t"
                                      fields {
                                        name: "i32"
                                        type {
                                          scalar: INT32
                                        }
                                      }
                                      )proto"));

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
      "struct socket_data_event_t {",
      "  int32_t i32;",
      "};",
      "static __inline int64_t pl_goid() {",
      "uint64_t current_pid_tgid = bpf_get_current_pid_tgid();",
      "const struct pid_goid_map_value_t* goid_ptr = pid_goid_map.lookup(&current_pid_tgid);",
      "return (goid_ptr == NULL) ? -1 : goid_ptr->goid_;",
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
  };

  std::vector<std::string> code_lines = absl::StrSplit(bcc_program.code, "\n");
  EXPECT_THAT(code_lines, ElementsAreArray(expected_code_lines));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
