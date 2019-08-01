#include <arrow/array.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <memory>
#include <sole.hpp>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/common/memory/memory.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using google::protobuf::TextFormat;
using table_store::Column;
using table_store::Table;
using table_store::schema::RowDescriptor;

class AddUDF : public udf::ScalarUDF {
 public:
  types::Float64Value Exec(udf::FunctionContext*, types::Int64Value v1, types::Float64Value v2) {
    return v1.val + v2.val;
  }
};

class MultiplyUDF : public udf::ScalarUDF {
 public:
  types::Float64Value Exec(udf::FunctionContext*, types::Float64Value v1, types::Int64Value v2) {
    return v1.val * v2.val;
  }
};

class ExecGraphTest : public ::testing::Test {
 protected:
  void SetUp() override {
    planpb::PlanFragment pf_pb;
    ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kPlanFragmentWithFourNodes, &pf_pb));
    ASSERT_OK(plan_fragment_->Init(pf_pb));

    auto udf_registry = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
    auto uda_registry = std::make_unique<udf::UDARegistry>("test_registry");
    auto table_store = std::make_shared<TableStore>();
    auto row_batch_queue = std::make_shared<RowBatchQueue>();

    exec_state_ = std::make_unique<ExecState>(udf_registry.get(), uda_registry.get(), table_store,
                                              row_batch_queue, sole::uuid4());
  }
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  std::unique_ptr<ExecState> exec_state_ = nullptr;
};

TEST_F(ExecGraphTest, basic) {
  ExecutionGraph e;
  auto udf_registry = std::make_unique<udf::ScalarUDFRegistry>("test");
  auto uda_registry = std::make_unique<udf::UDARegistry>("testUDA");
  auto plan_state = std::make_unique<plan::PlanState>(udf_registry.get(), uda_registry.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  table_store::schema::Relation relation(std::vector<types::DataType>({types::DataType::INT64}),
                                         std::vector<std::string>({"test"}));
  schema->AddRelation(1, relation);

  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get());

  // Check that the structure of the exec graph is correct.
  auto sources = e.sources();
  EXPECT_EQ(1, sources.size());
  auto root = e.node(sources[0]).ConsumeValueOrDie();
  EXPECT_TRUE(root->IsSource());
  auto root_children = root->children();
  EXPECT_EQ(2, root_children.size());
  EXPECT_TRUE(root_children[0]->IsProcessing());
  EXPECT_TRUE(root_children[1]->IsProcessing());
  EXPECT_EQ(1, root_children[0]->children().size());
  EXPECT_EQ(1, root_children[1]->children().size());
  EXPECT_TRUE(root_children[0]->children()[0]->IsSink());
  EXPECT_EQ(root_children[1]->children()[0], root_children[0]->children()[0]);
  EXPECT_EQ(0, root_children[1]->children()[0]->children().size());
}

TEST_F(ExecGraphTest, execute) {
  ExecutionGraph e;

  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto udf_registry = std::make_unique<udf::ScalarUDFRegistry>("testUDF");
  EXPECT_OK(udf_registry->Register<AddUDF>("add"));
  EXPECT_OK(udf_registry->Register<MultiplyUDF>("multiply"));
  auto uda_registry = std::make_unique<udf::UDARegistry>("testUDA");

  auto plan_state = std::make_unique<plan::PlanState>(udf_registry.get(), uda_registry.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = std::make_shared<Table>(rel);

  auto col1 = table->GetColumn(0);
  std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col1_in2 = {4, 5};

  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

  auto col2 = table->GetColumn(1);
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

  auto col3 = table->GetColumn(2);
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(col3->AddBatch(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(col3->AddBatch(types::ToArrow(col3_in2, arrow::default_memory_pool())));

  auto table_store = std::make_shared<TableStore>();
  table_store->AddTable("numbers", table);
  auto row_batch_queue = std::make_shared<RowBatchQueue>();
  auto exec_state_ = std::make_unique<ExecState>(udf_registry.get(), uda_registry.get(),
                                                 table_store, row_batch_queue, sole::uuid4());

  EXPECT_OK(exec_state_->AddScalarUDF(
      0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64})));
  EXPECT_OK(exec_state_->AddScalarUDF(
      1, "multiply",
      std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::INT64})));

  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get());

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Float64Value> out_in1 = {4.8, 16.4, 26.4};
  std::vector<types::Float64Value> out_in2 = {14.8, 12.4};
  EXPECT_EQ(2, output_table->NumBatches());
  EXPECT_TRUE(output_table->GetRowBatch(0, std::vector<int64_t>({0}), arrow::default_memory_pool())
                  .ConsumeValueOrDie()
                  ->ColumnAt(0)
                  ->Equals(types::ToArrow(out_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(output_table->GetRowBatch(1, std::vector<int64_t>({0}), arrow::default_memory_pool())
                  .ConsumeValueOrDie()
                  ->ColumnAt(0)
                  ->Equals(types::ToArrow(out_in2, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, execute_time) {
  ExecutionGraph e;

  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto udf_registry = std::make_unique<udf::ScalarUDFRegistry>("testUDF");
  EXPECT_OK(udf_registry->Register<AddUDF>("add"));
  EXPECT_OK(udf_registry->Register<MultiplyUDF>("multiply"));
  auto uda_registry = std::make_unique<udf::UDARegistry>("testUDA");

  auto plan_state = std::make_unique<plan::PlanState>(udf_registry.get(), uda_registry.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::TIME64NS, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::TIME64NS, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = std::make_shared<Table>(rel);

  auto col1 = table->GetColumn(0);
  std::vector<types::Time64NSValue> col1_in1 = {types::Time64NSValue(1), types::Time64NSValue(2),
                                                types::Time64NSValue(3)};
  std::vector<types::Time64NSValue> col1_in2 = {types::Time64NSValue(4), types::Time64NSValue(5)};

  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

  auto col2 = table->GetColumn(1);
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

  auto col3 = table->GetColumn(2);
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(col3->AddBatch(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(col3->AddBatch(types::ToArrow(col3_in2, arrow::default_memory_pool())));

  auto table_store = std::make_shared<TableStore>();
  table_store->AddTable("numbers", table);
  auto row_batch_queue = std::make_shared<RowBatchQueue>();

  auto exec_state_ = std::make_unique<ExecState>(udf_registry.get(), uda_registry.get(),
                                                 table_store, row_batch_queue, sole::uuid4());

  EXPECT_OK(exec_state_->AddScalarUDF(
      0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64})));
  EXPECT_OK(exec_state_->AddScalarUDF(
      1, "multiply",
      std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::INT64})));

  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get());

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Float64Value> out_in1 = {4.8, 16.4, 26.4};
  std::vector<types::Float64Value> out_in2 = {14.8, 12.4};
  EXPECT_EQ(2, output_table->NumBatches());
  EXPECT_TRUE(output_table->GetRowBatch(0, std::vector<int64_t>({0}), arrow::default_memory_pool())
                  .ConsumeValueOrDie()
                  ->ColumnAt(0)
                  ->Equals(types::ToArrow(out_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(output_table->GetRowBatch(1, std::vector<int64_t>({0}), arrow::default_memory_pool())
                  .ConsumeValueOrDie()
                  ->ColumnAt(0)
                  ->Equals(types::ToArrow(out_in2, arrow::default_memory_pool())));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
