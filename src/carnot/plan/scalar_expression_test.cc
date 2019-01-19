#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/plan/utils.h"

namespace pl {
namespace carnot {
namespace plan {

using google::protobuf::TextFormat;

TEST(ToString, values) {
  EXPECT_EQ("Function", ToString(planpb::ScalarExpression::kFunc));
  EXPECT_EQ("Column", ToString(planpb::ScalarExpression::kColumn));
  EXPECT_EQ("Value", ToString(planpb::ScalarExpression::kConstant));
}

class DummyTestUDF : udf::ScalarUDF {
 public:
  udf::Int64Value Exec(udf::FunctionContext*, udf::Float64Value, udf::Float64Value,
                       udf::Int64Value) {
    return 0;
  }
};

class ScalarExpressionTest : public ::testing::Test {
 public:
  ScalarExpressionTest() : state_(std::make_shared<udf::ScalarUDFRegistry>("test")) {
    state_.udf_registry()->RegisterOrDie<DummyTestUDF>("foobar");
    Relation rel0;
    rel0.AddColumn(planpb::INT64, "col0");
    rel0.AddColumn(planpb::FLOAT64, "col1");

    Relation rel1;
    rel1.AddColumn(planpb::INT64, "col0");
    rel1.AddColumn(planpb::FLOAT64, "col1");

    schema_.AddRelation(0, rel0);
    schema_.AddRelation(1, rel1);
  }
  virtual ~ScalarExpressionTest() {}

 protected:
  Schema schema_;
  CompilerState state_;
};

TEST(ColumnTest, basic_tests) {
  Column col;
  planpb::Column colpb;
  colpb.set_node(1);
  colpb.set_index(36);

  EXPECT_TRUE(col.Init(colpb).ok());
  EXPECT_EQ(1, col.NodeID());
  EXPECT_EQ(36, col.Index());
  EXPECT_EQ("node<1>::col[36]", col.DebugString());
  EXPECT_EQ(std::vector<const Column*>{&col}, col.ColumnDeps());
  EXPECT_EQ(std::vector<ScalarExpression*>{}, col.Deps());
}

TEST(ColumnDeathTest, no_init) {
  Column col;
  EXPECT_DEBUG_DEATH(col.NodeID(), "Not initialized");
  EXPECT_DEBUG_DEATH(col.Index(), "Not initialized");
}

TEST(ColumnDeathTest, double_init) {
  Column col;
  planpb::Column colpb;
  colpb.set_node(1);
  colpb.set_index(36);

  EXPECT_TRUE(col.Init(colpb).ok());
  EXPECT_DEBUG_DEATH(col.Init(colpb).ok(), "Already initialized");
}

TEST(ScalarValueTest, basic_tests_bool) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::BOOLEAN);
  sv_pb.set_bool_value(true);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_FALSE(sv.IsNull());
  EXPECT_EQ(planpb::BOOLEAN, sv.DataType());
  EXPECT_EQ(true, sv.BoolValue());
  EXPECT_EQ("true", sv.DebugString());
  EXPECT_EQ(std::vector<const Column*>{}, sv.ColumnDeps());
  EXPECT_EQ(std::vector<ScalarExpression*>{}, sv.Deps());
}

TEST(ScalarValueTest, basic_tests_bool_null) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::BOOLEAN);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_TRUE(sv.IsNull());
  EXPECT_EQ(planpb::BOOLEAN, sv.DataType());
  EXPECT_EQ("<null>", sv.DebugString());
  EXPECT_EQ(std::vector<const Column*>{}, sv.ColumnDeps());
}

TEST(ScalarValueTest, basic_tests_int64) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::INT64);
  sv_pb.set_int64_value(63);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_FALSE(sv.IsNull());
  EXPECT_EQ(planpb::INT64, sv.DataType());
  EXPECT_EQ(63, sv.Int64Value());
  EXPECT_EQ("63", sv.DebugString());
}

TEST(ScalarValueTest, basic_tests_int64_null) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::INT64);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_TRUE(sv.IsNull());
  EXPECT_EQ(planpb::INT64, sv.DataType());
  EXPECT_EQ("<null>", sv.DebugString());
}

TEST(ScalarValueTest, basic_tests_float64) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::FLOAT64);
  sv_pb.set_float64_value(3.14159);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_FALSE(sv.IsNull());
  EXPECT_EQ(planpb::FLOAT64, sv.DataType());
  EXPECT_DOUBLE_EQ(3.14159, sv.Float64Value());
  const std::string debug_string = sv.DebugString();
  EXPECT_TRUE(absl::StartsWith(debug_string, "3.14"));
  EXPECT_TRUE(absl::EndsWith(debug_string, "f"));
}

TEST(ScalarValueTest, basic_tests_float64_null) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::FLOAT64);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_TRUE(sv.IsNull());
  EXPECT_EQ(planpb::FLOAT64, sv.DataType());
  EXPECT_EQ("<null>", sv.DebugString());
}

TEST(ScalarValueTest, basic_tests_string) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::STRING);
  sv_pb.set_string_value("test string");

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_FALSE(sv.IsNull());
  EXPECT_EQ(planpb::STRING, sv.DataType());
  EXPECT_EQ("test string", sv.StringValue());
  EXPECT_EQ("\"test string\"", sv.DebugString());
}

TEST(ScalarValueTest, basic_tests_string_null) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::STRING);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_TRUE(sv.is_initialized());

  EXPECT_TRUE(sv.IsNull());
  EXPECT_EQ(planpb::STRING, sv.DataType());
  EXPECT_EQ("<null>", sv.DebugString());
}

TEST(ScalarValueDeathTest, double_init) {
  ScalarValue sv;
  planpb::ScalarValue sv_pb;
  sv_pb.set_data_type(planpb::STRING);

  EXPECT_TRUE(sv.Init(sv_pb).ok());
  EXPECT_DEBUG_DEATH(sv.Init(sv_pb).ok(), "Already initialized");
}

TEST(ScalarValueDeathTest, not_initialized) {
  ScalarValue sv;
  EXPECT_DEBUG_DEATH(sv.BoolValue(), "Not initialized");
  EXPECT_DEBUG_DEATH(sv.Int64Value(), "Not initialized");
  EXPECT_DEBUG_DEATH(sv.Float64Value(), "Not initialized");
  EXPECT_DEBUG_DEATH(sv.StringValue(), "Not initialized");
  EXPECT_DEBUG_DEATH(sv.IsNull(), "Not initialized");
}

TEST_F(ScalarExpressionTest, col_tests) {
  planpb::ScalarExpression se_pb;
  auto const_pb = se_pb.mutable_constant();
  const_pb->set_string_value("testing");
  const_pb->set_data_type(planpb::STRING);

  auto se_or_status = ScalarExpression::FromProto(se_pb);

  ASSERT_TRUE(se_or_status.ok());
  auto se = se_or_status.ConsumeValueOrDie();
  auto status = se->OutputDataType(state_, schema_);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(planpb::STRING, status.ValueOrDie());
}

const char* kFuncWithTwoColsProtoTxt = R"(
name: "foobar"
args {
  column {
    node: 0
    index: 1
  }
}
args {
  column {
    node: 1
    index: 1
  }
}
args {
  constant {
    data_type: INT64
    int64_value: 36
  }
})";

class ScalarFuncTest : public ScalarExpressionTest {
 public:
  ~ScalarFuncTest() override = default;
  void SetUp() override {
    planpb::ScalarFunc func_pb;
    ASSERT_TRUE(TextFormat::MergeFromString(kFuncWithTwoColsProtoTxt, &func_pb));
    ASSERT_TRUE(sf_.Init(func_pb).ok());
  }
  ScalarFunc sf_;
};

TEST_F(ScalarFuncTest, ColDeps) {
  const auto& cols = sf_.ColumnDeps();
  ASSERT_EQ(2, cols.size());
  EXPECT_EQ(0, cols[0]->NodeID());
  EXPECT_EQ(1, cols[0]->Index());
  EXPECT_EQ(1, cols[1]->NodeID());
  EXPECT_EQ(1, cols[1]->Index());
}

TEST_F(ScalarFuncTest, output_type) {
  auto res = sf_.OutputDataType(state_, schema_);
  ASSERT_TRUE(res.ok());
  EXPECT_EQ(planpb::INT64, res.ConsumeValueOrDie());
}

TEST_F(ScalarFuncTest, expression_type) {
  EXPECT_EQ(planpb::ScalarExpression::kFunc, sf_.ExpressionType());
}

TEST_F(ScalarFuncTest, deps) {
  const auto deps = sf_.Deps();
  ASSERT_EQ(3, deps.size());
  EXPECT_EQ(planpb::ScalarExpression::kColumn, deps[0]->ExpressionType());
  EXPECT_EQ(planpb::ScalarExpression::kColumn, deps[1]->ExpressionType());
  EXPECT_EQ(planpb::ScalarExpression::kConstant, deps[2]->ExpressionType());
}

TEST_F(ScalarFuncTest, debug_string) {
  EXPECT_EQ("fn:foobar(node<0>::col[1],node<1>::col[1],36)", sf_.DebugString());
}

TEST(ScalarExpressionWalker, walk_node_graph) {
  planpb::ScalarExpression se_pb;
  EXPECT_TRUE(TextFormat::MergeFromString(kFuncWithTwoColsProtoTxt, se_pb.mutable_func()));
  EXPECT_EQ(planpb::ScalarExpression::kFunc, se_pb.value_case());
  auto se = ScalarExpression::FromProto(se_pb);
  std::vector<int64_t> col_node_ids;
  int val_func_call_count = 0;
  auto col_count = ScalarExpressionWalker<int>()
                       .OnColumn([&](auto& col, auto&) {
                         col_node_ids.push_back(col.NodeID());
                         return 1;
                       })
                       .OnScalarFunc([&](auto& func, auto& child_values) {
                         EXPECT_EQ("foobar", func.name());
                         int sum = 0;
                         for (auto val : child_values) {
                           sum += val;
                         }
                         return sum;
                       })
                       .OnScalarValue([&](const auto& val, auto&) -> int {
                         EXPECT_EQ(planpb::INT64, val.DataType());
                         EXPECT_EQ(36, val.Int64Value());

                         ++val_func_call_count;
                         return 0;
                       })
                       .Walk(*(se.ConsumeValueOrDie().get()));
  ASSERT_TRUE(col_count.ok());
  EXPECT_EQ(2, col_count.ValueOrDie());
  EXPECT_EQ(1, val_func_call_count);
  EXPECT_EQ(std::vector<int64_t>({0, 1}), col_node_ids);
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
