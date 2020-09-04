#include "src/carnot/exec/exec_graph.h"

#include <arrow/array.h>
#include <arrow/memory_pool.h>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/test_utils.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
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

class BaseExecGraphTest : public ::testing::Test {
 protected:
  void SetUpExecState() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    func_registry_->RegisterOrDie<AddUDF>("add");
    func_registry_->RegisterOrDie<MultiplyUDF>("multiply");

    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, sole::uuid4());
  }

  std::unique_ptr<udf::Registry> func_registry_;
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  std::unique_ptr<ExecState> exec_state_ = nullptr;
};

class ExecGraphTest : public BaseExecGraphTest {
 protected:
  void SetUp() override {
    SetUpPlanFragment();
    SetUpExecState();
  }

  void SetUpPlanFragment() {
    planpb::PlanFragment pf_pb;
    ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kPlanFragmentWithFourNodes, &pf_pb));
    ASSERT_OK(plan_fragment_->Init(pf_pb));
  }
};

TEST_F(ExecGraphTest, basic) {
  ExecutionGraph e;
  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  table_store::schema::Relation relation(std::vector<types::DataType>({types::DataType::INT64}),
                                         std::vector<std::string>({"test"}));
  schema->AddRelation(1, relation);

  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

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
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create(rel);

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

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                                 MockResultSinkStubGenerator, sole::uuid4());

  EXPECT_OK(exec_state_->AddScalarUDF(
      0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64})));
  EXPECT_OK(exec_state_->AddScalarUDF(
      1, "multiply",
      std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::INT64})));

  ExecutionGraph e;
  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

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
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto func_registry = std::make_unique<udf::Registry>("testUDF");
  EXPECT_OK(func_registry->Register<AddUDF>("add"));
  EXPECT_OK(func_registry->Register<MultiplyUDF>("multiply"));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry.get());
  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::TIME64NS, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::TIME64NS, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create(rel);

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

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);

  auto exec_state_ = std::make_unique<ExecState>(func_registry.get(), table_store,
                                                 MockResultSinkStubGenerator, sole::uuid4());

  EXPECT_OK(exec_state_->AddScalarUDF(
      0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64})));
  EXPECT_OK(exec_state_->AddScalarUDF(
      1, "multiply",
      std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::INT64})));

  ExecutionGraph e;
  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

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

TEST_F(ExecGraphTest, two_limits_dont_interfere) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(
      TextFormat::MergeFromString(planpb::testutils::kPlanWithTwoSourcesWithLimits, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create(rel);

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

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                                 MockResultSinkStubGenerator, sole::uuid4());

  ExecutionGraph e;
  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table1 = exec_state_->table_store()->GetTable("output1");
  auto output_table2 = exec_state_->table_store()->GetTable("output2");
  std::vector<types::Int64Value> out_col1 = {1, 2};
  std::vector<types::BoolValue> out_col2 = {true, false};
  std::vector<types::Float64Value> out_col3 = {1.4, 6.2};
  EXPECT_EQ(1, output_table1->NumBatches());
  EXPECT_EQ(1, output_table2->NumBatches());
  auto out_rb1 =
      output_table1->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  auto out_rb2 =
      output_table2->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(out_rb1->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb1->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb1->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb2->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb2->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb2->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, limit_w_multiple_srcs) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kOneLimit3Sources, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create(rel);

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

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                                 MockResultSinkStubGenerator, sole::uuid4());

  ExecutionGraph e;
  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Int64Value> out_col1 = {1, 2};
  std::vector<types::BoolValue> out_col2 = {true, false};
  std::vector<types::Float64Value> out_col3 = {1.4, 6.2};
  EXPECT_EQ(1, output_table->NumBatches());
  auto out_rb =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(out_rb->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, two_sequential_limits) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kTwoSequentialLimits, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create(rel);

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

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                                 MockResultSinkStubGenerator, sole::uuid4());

  ExecutionGraph e;
  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Int64Value> out_col1 = {1, 2};
  std::vector<types::BoolValue> out_col2 = {true, false};
  std::vector<types::Float64Value> out_col3 = {1.4, 6.2};
  EXPECT_EQ(1, output_table->NumBatches());
  auto out_rb =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1, 2}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(out_rb->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
}

class YieldingExecGraphTest : public BaseExecGraphTest {
 protected:
  void SetUp() { SetUpExecState(); }
};

TEST_F(YieldingExecGraphTest, yield) {
  ExecutionGraph e;
  e.testing_set_exec_state(exec_state_.get());

  RowDescriptor output_rd({types::DataType::INT64});
  MockSourceNode yielding_source(output_rd);
  MockSourceNode non_yielding_source(output_rd);

  FakePlanNode yielding_plan_node(1);
  FakePlanNode non_yielding_plan_node(2);

  EXPECT_CALL(yielding_source, InitImpl(::testing::_));
  EXPECT_CALL(non_yielding_source, InitImpl(::testing::_));

  ASSERT_OK(yielding_source.Init(yielding_plan_node, output_rd, {}));
  ASSERT_OK(non_yielding_source.Init(non_yielding_plan_node, output_rd, {}));

  e.AddNode(1, &yielding_source);
  e.AddNode(2, &non_yielding_source);

  auto set_eos = [&](ExecState*) { non_yielding_source.SendEOS(); };

  // Setup
  EXPECT_CALL(non_yielding_source, PrepareImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(non_yielding_source, OpenImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(yielding_source, PrepareImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(yielding_source, OpenImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  // Non-yielding
  EXPECT_CALL(non_yielding_source, NextBatchReady())
      .Times(3)
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(false));

  EXPECT_CALL(non_yielding_source, GenerateNextImpl(::testing::_))
      .Times(2)
      .WillOnce(::testing::Return(Status::OK()))
      .WillOnce(::testing::DoAll(::testing::Invoke(set_eos), ::testing::Return(Status::OK())));

  // YieldingSource still gets NextBatchReady calls.
  EXPECT_CALL(yielding_source, NextBatchReady())
      .Times(2)
      .WillOnce(::testing::Return(false))
      .WillOnce(::testing::Return(false));

  // We don't set expectations on GenerateNextImpl for the yielding source, because
  // in TSAN the timeout may or may not occur resulting in different outputs.

  // Cleanup
  EXPECT_CALL(non_yielding_source, CloseImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(yielding_source, CloseImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  std::thread exec_thread([&] { ASSERT_OK(e.Execute()); });

  // Prod yielding node
  e.Continue();
  exec_thread.join();
}

TEST_F(ExecGraphTest, execute_with_two_limits) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(
      TextFormat::MergeFromString(planpb::testutils::kPlanWithTwoSourcesWithLimits, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create(rel);

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

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                                 MockResultSinkStubGenerator, sole::uuid4());

  ExecutionGraph e;
  auto s = e.Init(schema, plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table_1 = exec_state_->table_store()->GetTable("output1");
  auto output_table_2 = exec_state_->table_store()->GetTable("output2");
  std::vector<types::Float64Value> out_in1 = {1.4, 6.2};
  EXPECT_EQ(1, output_table_1->NumBatches());
  EXPECT_EQ(1, output_table_2->NumBatches());
  EXPECT_EQ(3, output_table_1->NumColumns());
  EXPECT_EQ(3, output_table_2->NumColumns());
  EXPECT_TRUE(
      output_table_1->GetRowBatch(0, std::vector<int64_t>({2}), arrow::default_memory_pool())
          .ConsumeValueOrDie()
          ->ColumnAt(0)
          ->Equals(types::ToArrow(out_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(
      output_table_2->GetRowBatch(0, std::vector<int64_t>({2}), arrow::default_memory_pool())
          .ConsumeValueOrDie()
          ->ColumnAt(0)
          ->Equals(types::ToArrow(out_in1, arrow::default_memory_pool())));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
