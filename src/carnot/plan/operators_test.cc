#include <gtest/gtest.h>
#include <iostream>

#include "src/carnot/plan/operators.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace plan {

using table_store::schema::Relation;
using table_store::schema::Schema;

class DummyTestUDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(udf::FunctionContext*, types::BoolValue, types::Float64Value) { return 0; }
};

class OperatorTest : public ::testing::Test {
 public:
  OperatorTest() {
    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test");
    uda_registry_ = std::make_unique<udf::UDARegistry>("testUDA");
    state_ = std::make_unique<PlanState>(udf_registry_.get(), uda_registry_.get());

    state_->udf_registry()->RegisterOrDie<DummyTestUDF>("testUdf");
    Relation rel0;
    rel0.AddColumn(types::INT64, "col0");
    rel0.AddColumn(types::FLOAT64, "col1");

    Relation rel1;
    rel1.AddColumn(types::INT64, "col0");
    rel1.AddColumn(types::FLOAT64, "col1");

    Relation rel2;
    rel2.AddColumn(types::DataType::FLOAT64, "abc");
    rel2.AddColumn(types::DataType::INT64, "time_");

    Relation rel3;
    rel3.AddColumn(types::DataType::INT64, "time_");
    rel3.AddColumn(types::DataType::INT64, "xyz");

    Relation rel4;
    rel4.AddColumn(types::DataType::FLOAT64, "time_");
    rel4.AddColumn(types::DataType::INT64, "xyz");

    schema_.AddRelation(0, rel0);
    schema_.AddRelation(1, rel1);
    schema_.AddRelation(2, rel2);
    schema_.AddRelation(3, rel3);
    schema_.AddRelation(4, rel4);
  }

  ~OperatorTest() override = default;

 protected:
  Schema schema_;
  std::unique_ptr<PlanState> state_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
};

TEST_F(OperatorTest, from_proto_map) {
  auto map_pb = planpb::testutils::CreateTestMap1PB();
  auto map_op = Operator::FromProto(map_pb, 1);
  EXPECT_EQ(1, map_op->id());
  EXPECT_TRUE(map_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::MAP_OPERATOR, map_op->op_type());
}

TEST_F(OperatorTest, from_proto_mem_src) {
  auto src_pb = planpb::testutils::CreateTestSource1PB();
  auto src_op = Operator::FromProto(src_pb, 1);
  EXPECT_EQ(1, src_op->id());
  EXPECT_TRUE(src_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::MEMORY_SOURCE_OPERATOR, src_op->op_type());
}

TEST_F(OperatorTest, from_proto_mem_sink) {
  auto sink_pb = planpb::testutils::CreateTestSink1PB();
  auto sink_op = Operator::FromProto(sink_pb, 1);
  EXPECT_EQ(1, sink_op->id());
  EXPECT_TRUE(sink_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::MEMORY_SINK_OPERATOR, sink_op->op_type());
}

TEST_F(OperatorTest, from_proto_grpc_source) {
  auto sink_pb = planpb::testutils::CreateTestGrpcSource1PB();
  auto sink_op = Operator::FromProto(sink_pb, 1);
  EXPECT_EQ(1, sink_op->id());
  EXPECT_TRUE(sink_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::GRPC_SOURCE_OPERATOR, sink_op->op_type());
}

TEST_F(OperatorTest, from_proto_grpc_sink) {
  auto sink_pb = planpb::testutils::CreateTestGrpcSink1PB();
  auto sink_op = Operator::FromProto(sink_pb, 1);
  EXPECT_EQ(1, sink_op->id());
  EXPECT_TRUE(sink_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::GRPC_SINK_OPERATOR, sink_op->op_type());
}

TEST_F(OperatorTest, from_proto_blocking_agg) {
  auto agg_pb = planpb::testutils::CreateTestBlockingAgg1PB();
  auto agg_op = Operator::FromProto(agg_pb, 1);
  EXPECT_EQ(1, agg_op->id());
  EXPECT_TRUE(agg_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::BLOCKING_AGGREGATE_OPERATOR, agg_op->op_type());
}

TEST_F(OperatorTest, from_proto_filter) {
  auto filter_pb = planpb::testutils::CreateTestFilter1PB();
  auto filter_op = Operator::FromProto(filter_pb, 1);
  EXPECT_EQ(1, filter_op->id());
  EXPECT_TRUE(filter_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::FILTER_OPERATOR, filter_op->op_type());
}

TEST_F(OperatorTest, from_proto_zip_time_column) {
  auto zip_pb = planpb::testutils::CreateTestZip1PB();
  auto zip_op = Operator::FromProto(zip_pb, 1);
  EXPECT_EQ(1, zip_op->id());
  EXPECT_TRUE(zip_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::ZIP_OPERATOR, zip_op->op_type());
}

TEST_F(OperatorTest, from_proto_zip_no_time_column) {
  auto zip_pb = planpb::testutils::CreateTestZip2PB();
  auto zip_op = Operator::FromProto(zip_pb, 1);
  EXPECT_EQ(1, zip_op->id());
  EXPECT_TRUE(zip_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::ZIP_OPERATOR, zip_op->op_type());
}

TEST_F(OperatorTest, from_proto_zip_out_of_range) {
  auto zip_pb = planpb::testutils::CreateTestZipOutOfRange();
  auto zip_op = std::make_unique<ZipOperator>(1);
  auto s = zip_op->Init(zip_pb.zip_op());
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.msg(), "Output index 5 out of bounds for parent relation 0 of ZipOperator");
}

TEST_F(OperatorTest, from_proto_zip_mismatched) {
  auto zip_pb = planpb::testutils::CreateTestZipMismatched();
  auto zip_op = std::make_unique<ZipOperator>(1);
  auto s = zip_op->Init(zip_pb.zip_op());
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.msg(), "Time column index must be set for either all tables or no tables.");
}

TEST_F(OperatorTest, from_proto_limit) {
  auto limit_pb = planpb::testutils::CreateTestLimit1PB();
  auto limit_op = Operator::FromProto(limit_pb, 1);
  EXPECT_EQ(1, limit_op->id());
  EXPECT_TRUE(limit_op->is_initialized());
  EXPECT_EQ(planpb::OperatorType::LIMIT_OPERATOR, limit_op->op_type());
}

TEST_F(OperatorTest, output_relation_source) {
  auto src_pb = planpb::testutils::CreateTestSource1PB();
  auto src_op = Operator::FromProto(src_pb, 1);

  auto rel = src_op->OutputRelation(schema_, *state_, std::vector<int64_t>());
  EXPECT_EQ(1, rel.ValueOrDie().NumColumns());
  EXPECT_EQ(types::DataType::FLOAT64, rel.ValueOrDie().GetColumnType(0));
  EXPECT_EQ("usage", rel.ValueOrDie().GetColumnName(0));
}

TEST_F(OperatorTest, output_relation_source_inputs) {
  auto src_pb = planpb::testutils::CreateTestSource1PB();
  auto src_op = Operator::FromProto(src_pb, 1);

  auto rel = src_op->OutputRelation(schema_, *state_, std::vector<int64_t>({1}));
  EXPECT_FALSE(rel.ok());
  EXPECT_EQ(rel.msg(), "Source operator cannot have any inputs");
}

TEST_F(OperatorTest, output_relation_sink) {
  auto sink_pb = planpb::testutils::CreateTestSink1PB();
  auto sink_op = Operator::FromProto(sink_pb, 1);
  auto rel = sink_op->OutputRelation(schema_, *state_, std::vector<int64_t>());
  EXPECT_EQ(0, rel.ValueOrDie().NumColumns());
}

TEST_F(OperatorTest, output_relation_map) {
  auto map_pb = planpb::testutils::CreateTestMap1PB();
  auto map_op = Operator::FromProto(map_pb, 1);
  auto rel = map_op->OutputRelation(schema_, *state_, std::vector<int64_t>({1}));
  EXPECT_EQ(1, rel.ValueOrDie().NumColumns());
  EXPECT_EQ(types::DataType::INT64, rel.ValueOrDie().GetColumnType(0));
}

TEST_F(OperatorTest, output_relation_map_no_input) {
  auto map_pb = planpb::testutils::CreateTestMap1PB();
  auto map_op = Operator::FromProto(map_pb, 1);
  auto rel = map_op->OutputRelation(schema_, *state_, std::vector<int64_t>({}));
  EXPECT_FALSE(rel.ok());
  EXPECT_EQ(rel.msg(), "Map operator must have exactly one input");
}

TEST_F(OperatorTest, output_relation_map_missing_rel) {
  auto map_pb = planpb::testutils::CreateTestMap1PB();
  auto map_op = Operator::FromProto(map_pb, 1);
  auto rel = map_op->OutputRelation(schema_, *state_, std::vector<int64_t>({10}));
  EXPECT_FALSE(rel.ok());
  EXPECT_EQ(rel.msg(), "Missing relation (10) for input of Map");
}

TEST_F(OperatorTest, output_relation_blocking_agg_no_input) {
  auto agg_pb = planpb::testutils::CreateTestBlockingAgg1PB();
  auto agg_op = Operator::FromProto(agg_pb, 1);
  auto rel = agg_op->OutputRelation(schema_, *state_, std::vector<int64_t>({}));
  EXPECT_FALSE(rel.ok());
  EXPECT_EQ(rel.msg(), "BlockingAgg operator must have exactly one input");
}

TEST_F(OperatorTest, output_relation_blocking_agg_missing_rel) {
  auto agg_pb = planpb::testutils::CreateTestBlockingAgg1PB();
  auto agg_op = Operator::FromProto(agg_pb, 1);
  auto rel = agg_op->OutputRelation(schema_, *state_, std::vector<int64_t>({10}));
  EXPECT_FALSE(rel.ok());
  EXPECT_EQ(rel.msg(), "Missing relation (10) for input of BlockingAggregateOperator");
}

TEST_F(OperatorTest, output_relation_filter) {
  auto filter_pb = planpb::testutils::CreateTestFilter1PB();
  auto filter_op = Operator::FromProto(filter_pb, 1);

  auto rel = filter_op->OutputRelation(schema_, *state_, std::vector<int64_t>({0}));
  EXPECT_EQ(2, rel.ValueOrDie().NumColumns());
  EXPECT_EQ(types::DataType::INT64, rel.ValueOrDie().GetColumnType(0));
  EXPECT_EQ(types::DataType::FLOAT64, rel.ValueOrDie().GetColumnType(1));
}

TEST_F(OperatorTest, output_relation_limit) {
  auto limit_pb = planpb::testutils::CreateTestLimit1PB();
  auto limit_op = Operator::FromProto(limit_pb, 1);

  auto rel = limit_op->OutputRelation(schema_, *state_, std::vector<int64_t>({0}));
  EXPECT_EQ(2, rel.ValueOrDie().NumColumns());
  EXPECT_EQ(types::DataType::INT64, rel.ValueOrDie().GetColumnType(0));
  EXPECT_EQ(types::DataType::FLOAT64, rel.ValueOrDie().GetColumnType(1));
}

TEST_F(OperatorTest, output_relation_zip) {
  auto zip_pb = planpb::testutils::CreateTestZip1PB();
  auto zip_op = Operator::FromProto(zip_pb, 4);

  auto rel = zip_op->OutputRelation(schema_, *state_, std::vector<int64_t>({2, 3}));
  EXPECT_EQ(3, rel.ValueOrDie().NumColumns());
  EXPECT_EQ(types::DataType::FLOAT64, rel.ValueOrDie().GetColumnType(0));
  EXPECT_EQ(types::DataType::INT64, rel.ValueOrDie().GetColumnType(1));
  EXPECT_EQ(types::DataType::INT64, rel.ValueOrDie().GetColumnType(2));
  EXPECT_EQ("abc", rel.ValueOrDie().GetColumnName(0));
  EXPECT_EQ("time_", rel.ValueOrDie().GetColumnName(1));
  EXPECT_EQ("xyz", rel.ValueOrDie().GetColumnName(2));
}

TEST_F(OperatorTest, output_relation_zip_mismatched) {
  auto zip_pb = planpb::testutils::CreateTestZip1PB();
  auto zip_op = Operator::FromProto(zip_pb, 4);

  auto rel = zip_op->OutputRelation(schema_, *state_, std::vector<int64_t>({2, 4}));
  EXPECT_FALSE(rel.ok());
  EXPECT_EQ(rel.msg(), "Conflicting types for column (time_) in ZipOperator");
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
