#include "src/carnot/compiler/objects/pl_module.h"
#include "src/carnot/compiler/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::pl::table_store::schema::Relation;

const char* kRegInfoProto = R"proto(
scalar_udfs {
  name: "pl.equals"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
)proto";
class PLModuleTest : public QLObjectTest {
 protected:
  std::unique_ptr<compiler::RegistryInfo> SetUpRegistryInfo() {
    udfspb::UDFInfo udf_proto;
    google::protobuf::TextFormat::MergeFromString(kRegInfoProto, &udf_proto);

    auto info = std::make_unique<compiler::RegistryInfo>();
    PL_CHECK_OK(info->Init(udf_proto));
    return info;
  }

  std::unique_ptr<RelationMap> SetUpRelMap() {
    auto rel_map = std::make_unique<RelationMap>();
    rel_map->emplace("sequences", Relation(
                                      {
                                          types::TIME64NS,
                                          types::FLOAT64,
                                          types::FLOAT64,
                                      },
                                      {"time_", "xmod10", "PIx"}));
    return rel_map;
  }

  void SetUp() override {
    QLObjectTest::SetUp();
    info_ = SetUpRegistryInfo();
    compiler_state_ = std::make_unique<CompilerState>(SetUpRelMap(), info_.get(), time_now_);
    module_ = PLModule::Create(graph.get(), compiler_state_.get()).ConsumeValueOrDie();
  }

  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
  std::unique_ptr<RegistryInfo> info_;
  std::shared_ptr<PLModule> module_;
};

TEST_F(PLModuleTest, ModuleFindAttributeFromRegistryInfo) {
  auto attr_or_s = module_->GetAttribute(ast, "equals");

  ASSERT_OK(attr_or_s);
  QLObjectPtr attr_object = attr_or_s.ConsumeValueOrDie();

  ASSERT_FALSE(attr_object->HasNode());
  ASSERT_TRUE(attr_object->type_descriptor().type() == QLObjectType::kFunction);
  auto result_or_s =
      std::static_pointer_cast<FuncObject>(attr_object)->Call({}, ast, ast_visitor.get());
  ASSERT_OK(result_or_s);
  auto ql_object = result_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(Match(ql_object->node(), Func()));

  FuncIR* func = static_cast<FuncIR*>(ql_object->node());
  EXPECT_EQ(func->carnot_op_name(), "equals");
}

TEST_F(PLModuleTest, AttributeNotFound) {
  std::string attribute = "bar";
  auto attr_or_s = module_->GetAttribute(ast, attribute);

  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status(), HasCompilerError("'pl' object has no attribute .*$0", attribute));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
