#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "src/carnot/plan/scalar_expression.h"

namespace pl {
namespace carnot {
namespace plan {

TEST(ColumnTest, basic_tests) {
  Column col;
  planpb::Column colpb;
  colpb.set_node(1);
  colpb.set_index(36);

  EXPECT_TRUE(col.Init(colpb).ok());
  EXPECT_EQ(1, col.NodeID());
  EXPECT_EQ(36, col.Index());
  EXPECT_EQ("node<1>::col[36]", col.DebugString());
  EXPECT_EQ(std::vector<Column*>{&col}, col.ColumnDeps());
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
  EXPECT_EQ(std::vector<Column*>{}, sv.ColumnDeps());
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
  EXPECT_EQ(std::vector<Column*>{}, sv.ColumnDeps());
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

TEST(ScalarExpression, col_tests) {
  planpb::ScalarExpression se_pb;
  auto const_pb = se_pb.mutable_constant();
  const_pb->set_string_value("testing");
  const_pb->set_data_type(planpb::STRING);

  auto se_or_status = ScalarExpression::FromProto(se_pb);

  ASSERT_TRUE(se_or_status.ok());
  Schema s;
  auto se = se_or_status.ConsumeValueOrDie();
  auto status = se->OutputDataType(s);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(planpb::STRING, status.ValueOrDie());
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
