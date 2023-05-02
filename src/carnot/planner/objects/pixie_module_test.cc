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

#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/qlobject.h"
#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/type_object.h"
#include "src/carnot/planner/objects/var_table.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::px::table_store::schema::Relation;

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
    PX_CHECK_OK(info_->Init(udf_proto));
    udfspb::UDTFSourceSpec spec;
    google::protobuf::TextFormat::MergeFromString(kUDTFSourcePb, &spec);
    info_->AddUDTF(spec);
    udfspb::UDTFSourceSpec spec2;
    google::protobuf::TextFormat::MergeFromString(kUDTFDefaultValueTestPb, &spec2);
    info_->AddUDTF(spec2);

    compiler_state_ = std::make_unique<CompilerState>(
        SetUpRelMap(), /* sensitive_columns */ SensitiveColumnMap{}, info_.get(),
        /* time_now */ time_now_,
        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
        /* redaction_options */ RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});

    module_ = PixieModule::Create(graph.get(), compiler_state_.get(), ast_visitor.get())
                  .ConsumeValueOrDie();
    var_table->Add("px", module_);
    var_table->Add("df",
                   Dataframe::Create(compiler_state_.get(), MakeMemSource(), ast_visitor.get())
                       .ConsumeValueOrDie());
  }

  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
  std::unique_ptr<RegistryInfo> info_;
  std::shared_ptr<PixieModule> module_;
};

TEST_F(PixieModuleTest, ModuleFindAttributeFromRegistryInfo) {
  ASSERT_OK_AND_ASSIGN(auto ql_object, ParseExpression("px.equals"));
  ASSERT_TRUE(FuncObject::IsFuncObject(ql_object));
}

TEST_F(PixieModuleTest, GetUDTFMethod) {
  ASSERT_OK_AND_ASSIGN(
      auto ql_object,
      ParseExpression("px.OpenNetworkConnections(upid='11285cdd-1de9-4ab1-ae6a-0ba08c8c676c')"));
  ASSERT_TRUE(Dataframe::IsDataframe(ql_object));
  auto df = static_cast<Dataframe*>(ql_object.get());
  ASSERT_MATCH(df->op(), UDTFSource());

  auto udtf = static_cast<UDTFSourceIR*>(df->op());
  EXPECT_EQ(udtf->func_name(), "OpenNetworkConnections");
  const auto& arg_values = udtf->arg_values();
  ASSERT_EQ(arg_values.size(), 1);
  EXPECT_MATCH(arg_values[0], UInt128Value());
  EXPECT_EQ(static_cast<UInt128IR*>(arg_values[0])->val(),
            md::UPID::ParseFromUUIDString("11285cdd-1de9-4ab1-ae6a-0ba08c8c676c")
                .ConsumeValueOrDie()
                .value());
}

TEST_F(PixieModuleTest, UDTFDefaultValueTest) {
  ASSERT_OK_AND_ASSIGN(auto ql_object, ParseExpression("px.DefaultValueTest()"));

  ASSERT_TRUE(Dataframe::IsDataframe(ql_object));
  auto df = static_cast<Dataframe*>(ql_object.get());
  ASSERT_MATCH(df->op(), UDTFSource());

  auto udtf = static_cast<UDTFSourceIR*>(df->op());
  EXPECT_EQ(udtf->func_name(), "DefaultValueTest");
  const auto& arg_values = udtf->arg_values();
  ASSERT_EQ(arg_values.size(), 1);
  auto uint_value = absl::MakeUint128(0, 1);
  EXPECT_MATCH(arg_values[0], UInt128Value());
  EXPECT_EQ(static_cast<UInt128IR*>(arg_values[0])->val(), uint_value);
}

TEST_F(PixieModuleTest, GetUDTFMethodBadArguements) {
  EXPECT_COMPILER_ERROR(ParseExpression("px.OpenNetworkConnections()"),
                        "missing 1 required positional arguments 'upid'");
}

TEST_F(PixieModuleTest, uuint128_conversion) {
  ASSERT_OK_AND_ASSIGN(auto uuint128_str_object,
                       ParseExpression("px.uint128('11285cdd-1de9-4ab1-ae6a-0ba08c8c676c')"));

  ASSERT_TRUE(ExprObject::IsExprObject(uuint128_str_object));
  auto expr = static_cast<ExprObject*>(uuint128_str_object.get());
  ASSERT_TRUE(UInt128IR::NodeMatches(expr->expr()));
  EXPECT_EQ(static_cast<UInt128IR*>(expr->expr())->val(),
            md::UPID::ParseFromUUIDString("11285cdd-1de9-4ab1-ae6a-0ba08c8c676c")
                .ConsumeValueOrDie()
                .value());
}

TEST_F(PixieModuleTest, uuint128_conversion_fails_on_invalid_string) {
  EXPECT_COMPILER_ERROR(ParseExpression("px.uint128('bad_uuid')"), ".* is not a valid UUID");
}

TEST_F(PixieModuleTest, dataframe_as_attribute) {
  ASSERT_OK_AND_ASSIGN(auto attr_object, module_->GetAttribute(ast, PixieModule::kDataframeOpID));
  EXPECT_TRUE(Dataframe::IsDataframe(attr_object));
}

TEST_F(PixieModuleTest, abs_time_test_with_tz) {
  ASSERT_OK_AND_ASSIGN(
      auto result,
      ParseExpression("px.strptime('2020-03-12 19:39:59 -0200', '%Y-%m-%d %H:%M:%S %z')"));
  ASSERT_TRUE(ExprObject::IsExprObject(result));

  auto expr = static_cast<ExprObject*>(result.get());
  ASSERT_TRUE(IntIR::NodeMatches(expr->expr()));
  EXPECT_EQ(static_cast<IntIR*>(expr->expr())->val(), 1584049199000000000);
}

TEST_F(PixieModuleTest, abs_time_test_expected_time_zone) {
  ASSERT_OK_AND_ASSIGN(
      auto result1,
      ParseExpression("px.strptime('2020-03-12 19:39:59 -0000', '%Y-%m-%d %H:%M:%S %z')"));
  ASSERT_OK_AND_ASSIGN(auto result2,
                       ParseExpression("px.strptime('2020-03-12 19:39:59', '%Y-%m-%d %H:%M:%S')"));

  auto expr1 = static_cast<ExprObject*>(result1.get());
  auto expr2 = static_cast<ExprObject*>(result2.get());
  // True value grabbed from online reference.
  EXPECT_EQ(static_cast<IntIR*>(expr1->expr())->val(), static_cast<IntIR*>(expr2->expr())->val());
}

TEST_F(PixieModuleTest, upid_constructor_test) {
  ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("px.make_upid(123, 456, 789)"));

  ASSERT_TRUE(ExprObject::IsExprObject(result));
  auto expr = static_cast<ExprObject*>(result.get());

  ASSERT_EQ(expr->expr()->type(), IRNodeType::kUInt128);
  ASSERT_EQ(expr->expr()->type_cast()->semantic_type(), types::ST_UPID);
  auto uint128 = static_cast<UInt128IR*>(expr->expr());
  auto upid = md::UPID(uint128->val());
  EXPECT_EQ(upid.asid(), 123);
  EXPECT_EQ(upid.pid(), 456);
  EXPECT_EQ(upid.start_ts(), 789);
}

TEST_F(PixieModuleTest, script_reference_test_no_args) {
  ASSERT_OK_AND_ASSIGN(auto result,
                       ParseExpression("px.script_reference(df.label, 'px/namespace')"));

  ASSERT_TRUE(ExprObject::IsExprObject(result));
  auto expr = static_cast<ExprObject*>(result.get());
  EXPECT_MATCH(expr->expr(), Func());
  auto func = static_cast<FuncIR*>(expr->expr());
  EXPECT_EQ(FuncIR::Opcode::non_op, func->opcode());
  EXPECT_EQ("_script_reference", func->func_name());
  auto args = func->all_args();
  ASSERT_EQ(2, args.size());
  EXPECT_MATCH(args[0], ColumnNode("label"));
  EXPECT_MATCH(args[1], String("px/namespace"));
}

TEST_F(PixieModuleTest, script_reference_test_with_args) {
  ASSERT_OK_AND_ASSIGN(auto result, ParseExpression(R"(
    px.script_reference(df.label, 'px/namespace', {
      'start_time': '-30s',
      'namespace': df.namespace_col,
    }))"));

  ASSERT_TRUE(ExprObject::IsExprObject(result));
  auto expr = static_cast<ExprObject*>(result.get());
  EXPECT_MATCH(expr->expr(), Func());
  auto func = static_cast<FuncIR*>(expr->expr());
  EXPECT_EQ(FuncIR::Opcode::non_op, func->opcode());
  EXPECT_EQ("_script_reference", func->func_name());
  auto args = func->all_args();
  ASSERT_EQ(6, args.size());
  EXPECT_MATCH(args[0], ColumnNode("label"));
  EXPECT_MATCH(args[1], String("px/namespace"));
  EXPECT_MATCH(args[2], String("start_time"));
  EXPECT_MATCH(args[3], String("-30s"));
  EXPECT_MATCH(args[4], String("namespace"));
  EXPECT_MATCH(args[5], ColumnNode("namespace_col"));
}

TEST_F(PixieModuleTest, format_duration) {
  // Test positive.
  {
    ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("px.format_duration(5 * 1000 * 1000)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), String());
    EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "5ms");
  }

  {
    ASSERT_OK_AND_ASSIGN(auto result,
                         ParseExpression("px.format_duration(5 * 1000 * 1000 * 1000)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), String());
    EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "5s");
  }

  {
    ASSERT_OK_AND_ASSIGN(auto result,
                         ParseExpression("px.format_duration(5 * 60 * 1000 * 1000 * 1000)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), String());
    EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "5m");
  }

  {
    ASSERT_OK_AND_ASSIGN(auto result,
                         ParseExpression("px.format_duration(60 * 60 * 1000 * 1000 * 1000)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), String());
    EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "1h");
  }

  {
    ASSERT_OK_AND_ASSIGN(auto result,
                         ParseExpression("px.format_duration(24 * 60 * 60 * 1000 * 1000 * 1000)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), String());
    EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "1d");
  }
  // Test negative value.
  {
    ASSERT_OK_AND_ASSIGN(auto result,
                         ParseExpression("px.format_duration(-5 * 60 * 1000 * 1000 * 1000)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), String());
    EXPECT_EQ(static_cast<StringIR*>(expr->expr())->str(), "-5m");
  }
}

TEST_F(PixieModuleTest, parse_duration) {
  // Test positive.
  {
    ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("px.parse_duration('5m')"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), Int());
    EXPECT_EQ(static_cast<IntIR*>(expr->expr())->val(), 300000000000);
  }

  // Test negative value.
  {
    ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("px.parse_duration('-5m')"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), Int());
    EXPECT_EQ(static_cast<IntIR*>(expr->expr())->val(), -300000000000);
  }
  // Test that bad format fails
  {
    EXPECT_COMPILER_ERROR(ParseExpression("px.parse_duration('randomstring')"),
                          "Time string is in wrong format.");
  }
}

TEST_F(PixieModuleTest, parse_time) {
  // Accepts string.
  {
    ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("px.parse_time('-5m')"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), Time());
    EXPECT_EQ(static_cast<TimeIR*>(expr->expr())->val(), time_now_ - 300000000000);
  }
  // Accepts integer.
  {
    ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("px.parse_time(100)"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), Time());
    EXPECT_EQ(static_cast<TimeIR*>(expr->expr())->val(), 100);
  }
  // Accepts time.
  {
    ASSERT_OK_AND_ASSIGN(
        auto result,
        ParseExpression(
            "px.parse_time(px.strptime('2020-03-12 19:39:59 -0200', '%Y-%m-%d %H:%M:%S %z'))"));
    ASSERT_TRUE(ExprObject::IsExprObject(result));
    auto expr = static_cast<ExprObject*>(result.get());
    ASSERT_MATCH(expr->expr(), Time());
    EXPECT_EQ(static_cast<TimeIR*>(expr->expr())->val(), 1584049199000000000);
  }

  // Test that bad format fails.
  {
    EXPECT_COMPILER_ERROR(ParseExpression("px.parse_time('randomstring')"), "Failed to parse time");
  }
}

TEST_F(PixieModuleTest, plugin_module) {
  EXPECT_COMPILER_ERROR(ParseExpression("px.plugin.start_time"), "No plugin config found");
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
