#include "src/stirling/dynamic_tracing/code_gen.h"

#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dynamic_tracing::ir::physical::MapStashAction;
using ::pl::stirling::dynamic_tracing::ir::physical::OutputAction;
using ::pl::stirling::dynamic_tracing::ir::physical::Printk;
using ::pl::stirling::dynamic_tracing::ir::physical::Probe;
using ::pl::stirling::dynamic_tracing::ir::physical::Register;
using ::pl::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::Struct;
using ::pl::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::pl::stirling::dynamic_tracing::ir::shared::BPFHelper;
using ::pl::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::pl::stirling::dynamic_tracing::ir::shared::VariableType;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
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

TEST(GenProbeTest, EntryProbe) {
  Probe probe;

  probe.set_name("probe_entry");

  ScalarVariable* var = nullptr;

  var = probe.add_vars();
  var->set_name("key");
  var->set_type(ScalarType::UINT32);
  var->set_builtin(BPFHelper::TGID);

  var = probe.add_vars();
  var->set_name("var");
  var->set_type(ScalarType::INT32);
  var->set_reg(Register::SP);

  {
    auto* var = probe.add_map_vars();
    var->set_name("map_var1");
    var->set_type("map_value_t");
    var->set_map_name("values");
    var->set_key_variable_name("var");
  }

  {
    auto* var = probe.add_member_vars();
    var->set_name("member_var1");
    var->set_type(ScalarType::INT32);
    var->set_struct_base("map_var1");
    var->set_is_struct_base_pointer(true);
    var->set_field("val");
  }

  StructVariable* st_var = probe.add_st_vars();

  st_var->set_name("st_var");
  st_var->set_type("socket_data_event_t");

  auto* fa = st_var->add_field_assignments();
  fa->set_field_name("i32");
  fa->set_variable_name("var");

  MapStashAction* map_stash_action = probe.add_map_stash_actions();

  map_stash_action->set_map_name("test");
  map_stash_action->set_key_variable_name("key");
  map_stash_action->set_value_variable_name("var");

  OutputAction* output_action = probe.add_output_actions();

  output_action->set_perf_buffer_name("data_events");
  output_action->set_variable_name("st_var");

  Printk* printk = probe.add_printks();
  printk->set_scalar("var");

  std::vector<std::string> expected = {
      "int probe_entry(struct pt_regs* ctx) {",
      "uint32_t key = bpf_get_current_pid_tgid() >> 32;",
      "int32_t var = (int32_t)PT_REGS_SP(ctx);",
      "struct map_value_t* map_var1 = values.lookup(&var);",
      "if (map_var1 == NULL) { return 0; }",
      "int32_t member_var1 = map_var1->val;",
      "struct socket_data_event_t st_var = {};",
      "st_var.i32 = var;",
      "test.update(&key, &var);",
      "data_events.perf_submit(ctx, &st_var, sizeof(st_var));",
      R"(bpf_trace_printk("var: %d\n", var);)",
      "return 0;",
      "}",
  };

  Struct st;
  st.set_name("socket_data_event_t");

  Struct::Field* field = st.add_fields();
  field->set_name("i32");
  field->mutable_type()->set_scalar(ScalarType::INT32);

  absl::flat_hash_map<std::string_view, const Struct*> structs = {{st.name(), &st}};
  ASSERT_OK_AND_THAT(GenProbe(structs, probe), ElementsAreArray(expected));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
