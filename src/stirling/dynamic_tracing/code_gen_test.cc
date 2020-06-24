#include "src/stirling/dynamic_tracing/code_gen.h"

#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dynamictracingpb::BPFHelper;
using ::pl::stirling::dynamictracingpb::MapStashAction;
using ::pl::stirling::dynamictracingpb::Register;
using ::pl::stirling::dynamictracingpb::ScalarType;
using ::pl::stirling::dynamictracingpb::Struct;
using ::pl::stirling::dynamictracingpb::Variable;
using ::pl::stirling::dynamictracingpb::VariableType;
using ::testing::ElementsAre;
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
  field->set_name("str");
  field->mutable_type()->set_scalar(ScalarType::STRING);

  field = st.add_fields();
  field->set_name("attr");
  field->mutable_type()->set_struct_type("attr_t");

  ASSERT_OK_AND_THAT(GenStruct(st, /*indent_size*/ 4), StrEq("struct socket_data_event_t {\n"
                                                             "    int32_t i32;\n"
                                                             "    int64_t i64;\n"
                                                             "    double double_val;\n"
                                                             "    void* msg;\n"
                                                             "    char* str;\n"
                                                             "    struct attr_t attr;\n"
                                                             "};\n"));
}

TEST(GenVariableTest, Register) {
  Variable var;

  var.set_name("var");
  var.set_val_type(ScalarType::VOID_POINTER);
  var.set_reg(Register::SP);

  ASSERT_OK_AND_THAT(GenVariable(var), StrEq("void* var = PT_REGS_SP(ctx);"));
}

TEST(GenVariableTest, Variable) {
  Variable var;

  var.set_name("var");
  var.set_val_type(ScalarType::INT32);

  auto* mem_var = var.mutable_memory();

  mem_var->set_base("sp");
  mem_var->set_offset(123);

  ASSERT_OK_AND_THAT(GenVariable(var), StrEq("int32_t var;\n"
                                             "bpf_probe_read(&var, sizeof(int32_t), sp + 123);\n"));
}

TEST(GenMapStashActionTest, StashMap) {
  MapStashAction action;

  action.set_map_name("test");
  action.set_builtin(BPFHelper::TGID);
  action.set_variable_name("foo");

  EXPECT_THAT(GenMapStashAction(action),
              ElementsAre("uint32_t tgid = bpf_get_current_pid_tgid() >> 32;\n",
                          "test.update(&tgid, &foo);\n"));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
