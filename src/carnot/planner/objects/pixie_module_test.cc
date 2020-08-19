#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/type_object.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
using ::pl::table_store::schema::Relation;

constexpr char kRegInfoProto[] = R"proto(
scalar_udfs {
  name: "equals"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
)proto";

constexpr char kUDTFSourcePb[] = R"proto(
name: "OpenNetworkConnections"
args {
  name: "upid"
  arg_type: UINT128
  semantic_type: ST_UPID
}
executor: UDTF_SUBSET_PEM
relation {
  columns {
    column_name: "time_"
    column_type: TIME64NS
  }
  columns {
    column_name: "fd"
    column_type: INT64
  }
  columns {
    column_name: "name"
    column_type: STRING
  }
}
)proto";

constexpr char kUDTFDefaultValueTestPb[] = R"proto(
name: "DefaultValueTest"
args {
  name: "upid"
  arg_type: UINT128
  semantic_type: ST_UPID
  default_value {
    data_type: UINT128
    uint128_value {
      high: 0
      low: 1
    }

  }
}
executor: UDTF_SUBSET_PEM
relation {
  columns {
    column_name: "time_"
    column_type: TIME64NS
  }
  columns {
    column_name: "fd"
    column_type: INT64
  }
  columns {
    column_name: "name"
    column_type: STRING
  }
}
)proto";

class PixieModuleTest : public QLObjectTest {
 protected:
  // std::unique_ptr<planner::RegistryInfo> SetUpRegistryInfo() {

  //   return info;
  // }

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

    udfspb::UDFInfo udf_proto;
    CHECK(google::protobuf::TextFormat::MergeFromString(kRegInfoProto, &udf_proto));

    info_ = std::make_unique<planner::RegistryInfo>();
    PL_CHECK_OK(info_->Init(udf_proto));
    udfspb::UDTFSourceSpec spec;
    google::protobuf::TextFormat::MergeFromString(kUDTFSourcePb, &spec);
    info_->AddUDTF(spec);
    udfspb::UDTFSourceSpec spec2;
    google::protobuf::TextFormat::MergeFromString(kUDTFDefaultValueTestPb, &spec2);
    info_->AddUDTF(spec2);

    compiler_state_ =
        std::make_unique<CompilerState>(SetUpRelMap(), info_.get(), time_now_, "result_addr");

    module_ = PixieModule::Create(graph.get(), compiler_state_.get(), ast_visitor.get())
                  .ConsumeValueOrDie();
  }

  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
  std::unique_ptr<RegistryInfo> info_;
  std::shared_ptr<PixieModule> module_;
};

TEST_F(PixieModuleTest, ModuleFindAttributeFromRegistryInfo) {
  auto attr_or_s = module_->GetAttribute(ast, "equals");

  ASSERT_OK(attr_or_s);
  QLObjectPtr attr_object = attr_or_s.ConsumeValueOrDie();

  ASSERT_FALSE(attr_object->HasNode());
  ASSERT_TRUE(attr_object->type_descriptor().type() == QLObjectType::kFunction);
  auto result_or_s = std::static_pointer_cast<FuncObject>(attr_object)->Call({}, ast);
  ASSERT_OK(result_or_s);
  auto ql_object = result_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(ql_object->node(), Func());

  FuncIR* func = static_cast<FuncIR*>(ql_object->node());
  EXPECT_EQ(func->carnot_op_name(), "equals");
}

TEST_F(PixieModuleTest, AttributeNotFound) {
  std::string attribute = "bar";
  auto attr_or_s = module_->GetAttribute(ast, attribute);

  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status(), HasCompilerError("'$1' object has no attribute .*$0", attribute,
                                                   PixieModule::kPixieModuleObjName));
}

TEST_F(PixieModuleTest, GetUDTFMethod) {
  std::string upid_value = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c";
  auto upid_str = MakeString(upid_value);
  std::string network_conns_udtf_name = "OpenNetworkConnections";
  auto method_or_s = module_->GetMethod(network_conns_udtf_name);

  ASSERT_OK(method_or_s);
  QLObjectPtr method_object = method_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(method_object->type_descriptor().type() == QLObjectType::kFunction);
  auto result_or_s = std::static_pointer_cast<FuncObject>(method_object)
                         ->Call(MakeArgMap({{"upid", upid_str}}, {}), ast);
  ASSERT_OK(result_or_s);
  auto ql_object = result_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  ASSERT_MATCH(ql_object->node(), UDTFSource());

  auto udtf = static_cast<UDTFSourceIR*>(ql_object->node());
  EXPECT_EQ(udtf->func_name(), network_conns_udtf_name);
  const auto& arg_values = udtf->arg_values();
  ASSERT_EQ(arg_values.size(), 1);
  auto upid = md::UPID::ParseFromUUIDString(upid_value).ConsumeValueOrDie();
  EXPECT_MATCH(arg_values[0], UInt128Value());
  EXPECT_EQ(static_cast<UInt128IR*>(arg_values[0])->val(), upid.value());
}

TEST_F(PixieModuleTest, UDTFDefaultValueTest) {
  std::string udtf_name = "DefaultValueTest";
  auto method_or_s = module_->GetMethod(udtf_name);

  ASSERT_OK(method_or_s);
  QLObjectPtr method_object = method_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(method_object->type_descriptor().type() == QLObjectType::kFunction);
  // No values.
  auto result_or_s = std::static_pointer_cast<FuncObject>(method_object)->Call({}, ast);
  ASSERT_OK(result_or_s);
  auto ql_object = result_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  ASSERT_MATCH(ql_object->node(), UDTFSource());

  auto udtf = static_cast<UDTFSourceIR*>(ql_object->node());
  EXPECT_EQ(udtf->func_name(), udtf_name);
  const auto& arg_values = udtf->arg_values();
  ASSERT_EQ(arg_values.size(), 1);
  auto uint_value = absl::MakeUint128(0, 1);
  EXPECT_MATCH(arg_values[0], UInt128Value());
  EXPECT_EQ(static_cast<UInt128IR*>(arg_values[0])->val(), uint_value);
}

TEST_F(PixieModuleTest, GetUDTFMethodBadArguements) {
  std::string network_conns_udtf_name = "OpenNetworkConnections";
  auto method_or_s = module_->GetMethod(network_conns_udtf_name);

  ASSERT_OK(method_or_s);
  QLObjectPtr method_object = method_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(method_object->type_descriptor().type() == QLObjectType::kFunction);
  auto result_or_s = std::static_pointer_cast<FuncObject>(method_object)->Call({}, ast);
  ASSERT_NOT_OK(result_or_s);
  EXPECT_THAT(result_or_s.status(),
              HasCompilerError("missing 1 required positional arguments 'upid'"));
}

TEST_F(PixieModuleTest, uuint128_conversion) {
  std::string uuint128_str = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c";
  auto uuint128_or_s = md::UPID::ParseFromUUIDString(uuint128_str);
  ASSERT_OK(uuint128_or_s) << "uuint128 should be valid.";
  auto expected_uuint128 = uuint128_or_s.ConsumeValueOrDie();

  auto method_or_s = module_->GetMethod(PixieModule::kUInt128ConversionId);
  ASSERT_OK(method_or_s);

  QLObjectPtr method_object = method_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(method_object->type_descriptor().type() == QLObjectType::kFunction);

  auto result_or_s = std::static_pointer_cast<FuncObject>(method_object)
                         ->Call(MakeArgMap({{"uuid", MakeString(uuint128_str)}}, {}), ast);
  ASSERT_OK(result_or_s);
  QLObjectPtr uuint128_str_object = result_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(uuint128_str_object->type_descriptor().type() == QLObjectType::kExpr);

  std::shared_ptr<ExprObject> expr = std::static_pointer_cast<ExprObject>(uuint128_str_object);
  ASSERT_EQ(expr->node()->type(), IRNodeType::kUInt128);
  EXPECT_EQ(static_cast<UInt128IR*>(expr->node())->val(), expected_uuint128.value());
}

TEST_F(PixieModuleTest, uuint128_conversion_fails_on_invalid_string) {
  std::string upid_str = "bad_uuid";

  auto method_or_s = module_->GetMethod(PixieModule::kUInt128ConversionId);
  ASSERT_OK(method_or_s);

  QLObjectPtr method_object = method_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(method_object->type_descriptor().type() == QLObjectType::kFunction);

  auto result_or_s = std::static_pointer_cast<FuncObject>(method_object)
                         ->Call(MakeArgMap({{"uuid", MakeString(upid_str)}}, {}), ast);
  ASSERT_NOT_OK(result_or_s);
  EXPECT_THAT(result_or_s.status(), HasCompilerError(".* is not a valid UUID"));
}

TEST_F(PixieModuleTest, dataframe_as_attribute) {
  auto attr_or_s = module_->GetAttribute(ast, PixieModule::kDataframeOpId);
  ASSERT_OK(attr_or_s);

  QLObjectPtr attr_object = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(attr_object->type_descriptor().type() == QLObjectType::kDataframe);
}

TEST_F(PixieModuleTest, abs_time_test_with_tz) {
  auto abs_time_or_s = module_->GetMethod(PixieModule::kAbsTimeOpId);
  ASSERT_OK(abs_time_or_s);

  std::shared_ptr<FuncObject> fn = abs_time_or_s.ConsumeValueOrDie();
  // Testing with a timezone that we probably won't encounter to make sure the time zone parsing
  // works.
  auto result_or_s = fn->Call({{},
                               {ToQLObject(MakeString("2020-03-12 19:39:59 -0200")),
                                ToQLObject(MakeString("%Y-%m-%d %H:%M:%S %z"))}},
                              ast);
  ASSERT_OK(result_or_s);
  QLObjectPtr result = result_or_s.ConsumeValueOrDie();
  ASSERT_EQ(result->type(), QLObjectType::kExpr);
  ASSERT_EQ(result->node()->type(), IRNodeType::kInt);
  // True value grabbed from online reference.
  EXPECT_EQ(static_cast<IntIR*>(result->node())->val(), 1584049199000000000);
}

TEST_F(PixieModuleTest, abs_time_test_expected_time_zone) {
  auto abs_time_or_s = module_->GetMethod(PixieModule::kAbsTimeOpId);
  ASSERT_OK(abs_time_or_s);

  std::shared_ptr<FuncObject> fn = abs_time_or_s.ConsumeValueOrDie();
  // Test to show that our time zone is UTC.
  auto result1_or_s = fn->Call({{},
                                {ToQLObject(MakeString("2020-03-12 19:39:59 -0000")),
                                 ToQLObject(MakeString("%Y-%m-%d %H:%M:%S %z"))}},
                               ast);

  auto result2_or_s = fn->Call({{},
                                {ToQLObject(MakeString("2020-03-12 19:39:59")),
                                 ToQLObject(MakeString("%Y-%m-%d %H:%M:%S"))}},
                               ast);

  ASSERT_OK(result1_or_s);
  ASSERT_OK(result2_or_s);
  QLObjectPtr result1 = result1_or_s.ConsumeValueOrDie();
  QLObjectPtr result2 = result2_or_s.ConsumeValueOrDie();
  ASSERT_EQ(result1->type(), QLObjectType::kExpr);
  ASSERT_EQ(result1->node()->type(), IRNodeType::kInt);
  ASSERT_EQ(result2->type(), QLObjectType::kExpr);
  ASSERT_EQ(result2->node()->type(), IRNodeType::kInt);
  // True value grabbed from online reference.
  EXPECT_EQ(static_cast<IntIR*>(result1->node())->val(),
            static_cast<IntIR*>(result2->node())->val());
}

TEST_F(PixieModuleTest, upid_constructor_test) {
  auto make_upid_or_s = module_->GetMethod(PixieModule::kMakeUPIDId);
  ASSERT_OK(make_upid_or_s);

  std::shared_ptr<FuncObject> fn = make_upid_or_s.ConsumeValueOrDie();
  // Test to show that our time zone is UTC.
  auto result_or_s = fn->Call(
      {{}, {ToQLObject(MakeInt(123)), ToQLObject(MakeInt(456)), ToQLObject(MakeInt(789))}}, ast);

  ASSERT_OK(result_or_s);
  QLObjectPtr result = result_or_s.ConsumeValueOrDie();
  ASSERT_EQ(result->type(), QLObjectType::kExpr);
  auto expr = static_cast<ExpressionIR*>(result->node());
  ASSERT_EQ(expr->type(), IRNodeType::kUInt128);
  ASSERT_EQ(expr->type_cast()->semantic_type(), types::ST_UPID);
  auto uint128 = static_cast<UInt128IR*>(expr);
  auto upid = md::UPID(uint128->val());
  EXPECT_EQ(upid.asid(), 123);
  EXPECT_EQ(upid.pid(), 456);
  EXPECT_EQ(upid.start_ts(), 789);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
