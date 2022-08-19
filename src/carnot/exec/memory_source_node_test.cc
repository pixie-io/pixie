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

#include "src/carnot/exec/memory_source_node.h"

#include <arrow/memory_pool.h>
#include <memory>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/types.pb.h"
namespace px {
namespace carnot {
namespace exec {

using table_store::Table;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;

class MemorySourceNodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);

    table_store::schema::Relation rel({types::DataType::BOOLEAN, types::DataType::TIME64NS},
                                      {"col1", "time_"});

    int64_t compaction_size = 2 * sizeof(bool) + 2 * sizeof(int64_t);
    cpu_table_ = std::make_shared<Table>("cpu", rel, 128 * 1024, compaction_size);
    exec_state_->table_store()->AddTable("cpu", cpu_table_);

    auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
    std::vector<types::BoolValue> col1_in1 = {true, false, true};
    std::vector<types::Time64NSValue> col2_in1 = {1, 2, 3};
    EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
    EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
    EXPECT_OK(cpu_table_->WriteRowBatch(rb1));

    auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
    std::vector<types::BoolValue> col1_in2 = {false, false};
    std::vector<types::Time64NSValue> col2_in2 = {5, 6};
    EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
    EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
    EXPECT_OK(cpu_table_->WriteRowBatch(rb2));

    exec_state_->table_store()->AddTable("empty", Table::Create("empty", rel));
  }

  std::shared_ptr<Table> cpu_table_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(MemorySourceNodeTest, basic) {
  auto op_proto = planpb::testutils::CreateTestSource1PB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 3, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({1, 2, 3})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({5, 6})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
  EXPECT_EQ(5, tester.node()->RowsProcessed());
  EXPECT_EQ(sizeof(int64_t) * 5, tester.node()->BytesProcessed());
}

TEST_F(MemorySourceNodeTest, empty_table) {
  auto op_proto = planpb::testutils::CreateTestSource1PB("empty");
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 0, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
  EXPECT_EQ(0, tester.node()->RowsProcessed());
  EXPECT_EQ(0, tester.node()->BytesProcessed());
}

TEST_F(MemorySourceNodeTest, added_batch) {
  auto op_proto = planpb::testutils::CreateTestSource1PB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 3, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({1, 2, 3})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({5, 6})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  auto rb1 = RowBatch(RowDescriptor(cpu_table_->GetRelation().col_types()), 3);
  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  std::vector<types::Time64NSValue> col2_in1 = {1, 2, 3};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(cpu_table_->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(cpu_table_->GetRelation().col_types()), 2);
  std::vector<types::BoolValue> col1_in2 = {false, false};
  std::vector<types::Time64NSValue> col2_in2 = {5, 6};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(cpu_table_->WriteRowBatch(rb2));
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());

  tester.Close();
  EXPECT_EQ(5, tester.node()->RowsProcessed());
  EXPECT_EQ(sizeof(int64_t) * 5, tester.node()->BytesProcessed());
}

TEST_F(MemorySourceNodeTest, range) {
  auto op_proto = planpb::testutils::CreateTestSourceRangePB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 1, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({3})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({5, 6})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
}

TEST_F(MemorySourceNodeTest, empty_range) {
  auto op_proto = planpb::testutils::CreateTestSourceEmptyRangePB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 0, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
  EXPECT_EQ(0, tester.node()->RowsProcessed());
  EXPECT_EQ(0, tester.node()->BytesProcessed());
}

TEST_F(MemorySourceNodeTest, all_range) {
  auto op_proto = planpb::testutils::CreateTestSourceAllRangePB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 1, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({3})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({5, 6})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
}

class MemorySourceNodeTabletTest : public ::testing::Test {
 protected:
  void SetUp() override {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);

    rel = table_store::schema::Relation({types::DataType::BOOLEAN, types::DataType::TIME64NS},
                                        {"col1", "time_"});

    std::shared_ptr<Table> tablet = Table::Create(table_name_, rel);
    AddValuesToTable(tablet.get());

    exec_state_->table_store()->AddTable(tablet, table_name_, table_id_, tablet_id_);
  }

  void AddValuesToTable(Table* table) {
    auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
    std::vector<types::BoolValue> col1_in1 = {true, false, true};
    std::vector<types::Time64NSValue> col2_in1 = {1, 2, 3};
    EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
    EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
    EXPECT_OK(table->WriteRowBatch(rb1));

    auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
    std::vector<types::BoolValue> col1_in2 = {false, false};
    std::vector<types::Time64NSValue> col2_in2 = {5, 6};
    EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
    EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
    EXPECT_OK(table->WriteRowBatch(rb2));
  }

  table_store::schema::Relation rel;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  std::string table_name_ = "cpu";
  uint64_t table_id_ = 987;
  types::TabletID tablet_id_ = "123";
};

// Test to make sure that memory source node can read in tablets that are not default.
TEST_F(MemorySourceNodeTabletTest, basic_tablet_test) {
  auto op_proto =
      planpb::testutils::CreateTestSourceWithTablets1PB(absl::Substitute("\"$0\"", tablet_id_));
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 3, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({1, 2, 3})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({5, 6})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
  EXPECT_EQ(5, tester.node()->RowsProcessed());
  EXPECT_EQ(sizeof(int64_t) * 5, tester.node()->BytesProcessed());
}

// Test to make sure that tablets don't interfere with each other.
TEST_F(MemorySourceNodeTabletTest, multiple_tablet_test) {
  types::TabletID new_tablet_id = "456";
  EXPECT_NE(tablet_id_, new_tablet_id);
  std::shared_ptr<Table> new_tablet = Table::Create(tablet_id_, rel);

  auto wrapper_batch_1 = std::make_unique<px::types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1 = std::make_shared<types::BoolValueColumnWrapper>(0);
  col_wrapper_1->Clear();

  auto col_wrapper_2 = std::make_shared<types::Time64NSValueColumnWrapper>(0);
  col_wrapper_2->Clear();

  wrapper_batch_1->push_back(col_wrapper_1);
  wrapper_batch_1->push_back(col_wrapper_2);

  EXPECT_OK(
      exec_state_->table_store()->AppendData(table_id_, new_tablet_id, std::move(wrapper_batch_1)));
  auto op_proto =
      planpb::testutils::CreateTestSourceWithTablets1PB(absl::Substitute("\"$0\"", new_tablet_id));
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 0, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  EXPECT_EQ(0, tester.node()->RowsProcessed());
  EXPECT_EQ(0, tester.node()->BytesProcessed());
}

using MemorySourceNodeTabletDeathTest = MemorySourceNodeTabletTest;
TEST_F(MemorySourceNodeTabletDeathTest, missing_tablet_fails) {
  types::TabletID non_existant_tablet_value = "223";

  EXPECT_NE(non_existant_tablet_value, tablet_id_);
  auto op_proto = planpb::testutils::CreateTestSourceWithTablets1PB(
      absl::Substitute("\"$0\"", non_existant_tablet_value));
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});
  auto exec_node_ = std::make_unique<MemorySourceNode>();
  const auto* casted_plan_node = static_cast<const plan::MemorySourceOperator*>(plan_node.get());
  // copy the plan node to local object;
  auto plan_node_ = std::make_unique<plan::MemorySourceOperator>(*casted_plan_node);
  EXPECT_OK(exec_node_->Init(*plan_node_, output_rd, std::vector<RowDescriptor>({})));
  EXPECT_OK(exec_node_->Prepare(exec_state_.get()));

  EXPECT_DEBUG_DEATH(EXPECT_NOT_OK(exec_node_->Open(exec_state_.get())), "");
}

TEST_F(MemorySourceNodeTest, streaming) {
  auto op_proto = planpb::testutils::CreateTestStreamingSource1PB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  EXPECT_TRUE(static_cast<plan::MemorySourceOperator*>(plan_node.get())->streaming());
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 3, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({1, 2, 3})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({5, 6})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  EXPECT_FALSE(tester.node()->NextBatchReady());

  // Simulate stirling still writing to the table.
  auto rb1 = RowBatch(RowDescriptor(cpu_table_->GetRelation().col_types()), 4);
  std::vector<types::BoolValue> col1_in1 = {true, false, true, true};
  std::vector<types::Time64NSValue> col2_in1 = {7, 8, 9, 10};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(cpu_table_->WriteRowBatch(rb1));

  EXPECT_TRUE(tester.node()->NextBatchReady());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 4, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({7, 8, 9, 10})
          .get());
  EXPECT_FALSE(tester.node()->NextBatchReady());
  // NOTE: only the outside loop should determine that batches should remain.
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.Close();
}

TEST_F(MemorySourceNodeTest, table_compact_between_open_and_exec) {
  auto op_proto = planpb::testutils::CreateTestSourceRangePB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  RowDescriptor output_rd({types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  // Force a table compaction between MemorySource::Open and MemorySource::Exec.
  EXPECT_OK(cpu_table_->CompactHotToCold(arrow::default_memory_pool()));

  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Time64NSValue>({3, 5})
          .get());
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  // Force a second compaction to check between Exec and a subsequent Exec.
  EXPECT_OK(cpu_table_->CompactHotToCold(arrow::default_memory_pool()));

  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 1, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Time64NSValue>({6})
          .get());
  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();
}

struct MemorySourceTestCase {
  std::string name;
  std::vector<std::vector<int64_t>> initial_time_batches;
  std::optional<int64_t> start_time;
  std::optional<int64_t> stop_time;
  bool streaming;

  struct Action {
    enum ActionType {
      ExpectBatch,
      AddBatchToTable,
      CompactTable,
    } type;
    std::vector<int64_t> times = {};
    bool eow = false;
    bool eos = false;
  };
  std::vector<Action> actions;

  size_t expect_n_rows;
  bool expect_batches_remaining;
};
using ActionType = MemorySourceTestCase::Action::ActionType;

class ParamMemorySourceNodeTest : public ::testing::Test,
                                  public ::testing::WithParamInterface<MemorySourceTestCase> {
  using Tester = exec::ExecNodeTester<MemorySourceNode, plan::MemorySourceOperator>;

 protected:
  void SetUp() override {
    ::testing::Test::SetUp();
    test_case_ = GetParam();
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);

    rel_ = std::make_unique<table_store::schema::Relation>(
        std::vector<types::DataType>{types::DataType::TIME64NS}, std::vector<std::string>{"time_"});

    int64_t compaction_size = 2 * sizeof(int64_t);
    cpu_table_ = std::make_shared<Table>("cpu", *rel_, 128 * 1024, compaction_size);
    exec_state_->table_store()->AddTable("cpu", cpu_table_);

    planpb::Operator op;
    op.set_op_type(planpb::OperatorType::MEMORY_SOURCE_OPERATOR);
    auto* mem_src_op = op.mutable_mem_source_op();
    mem_src_op->set_name("cpu");
    mem_src_op->add_column_idxs(0);
    mem_src_op->add_column_types(types::DataType::TIME64NS);
    mem_src_op->add_column_names("time_");
    mem_src_op->set_streaming(test_case_.streaming);
    if (test_case_.start_time.has_value()) {
      auto* start_time = mem_src_op->mutable_start_time();
      start_time->set_value(test_case_.start_time.value());
    }
    if (test_case_.stop_time.has_value()) {
      auto* stop_time = mem_src_op->mutable_stop_time();
      stop_time->set_value(test_case_.stop_time.value());
    }
    plan_node_ = plan::MemorySourceOperator::FromProto(op, 1);

    for (const auto& batch : test_case_.initial_time_batches) {
      WriteBatch(batch);
    }

    output_rd_ =
        std::make_unique<RowDescriptor>(std::vector<types::DataType>{types::DataType::TIME64NS});
    tester_ = std::make_unique<Tester>(*plan_node_, *output_rd_, std::vector<RowDescriptor>({}),
                                       exec_state_.get());
  }

  void WriteBatch(const std::vector<int64_t>& times) {
    auto rb = RowBatch(RowDescriptor(rel_->col_types()), static_cast<int64_t>(times.size()));
    std::vector<types::Time64NSValue> col(times.begin(), times.end());
    EXPECT_OK(rb.AddColumn(types::ToArrow(col, arrow::default_memory_pool())));
    EXPECT_OK(cpu_table_->WriteRowBatch(rb));
  }

  void ExpectBatch(const std::vector<int64_t>& times, bool eow, bool eos) {
    tester_->GenerateNextResult().ExpectRowBatch(
        RowBatchBuilder(*output_rd_, times.size(), eow, eos)
            .AddColumn<types::Time64NSValue>(
                std::vector<types::Time64NSValue>{times.begin(), times.end()})
            .get());
  }

  MemorySourceTestCase test_case_;
  std::unique_ptr<table_store::schema::Relation> rel_;
  std::shared_ptr<Table> cpu_table_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  std::unique_ptr<Tester> tester_;
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<RowDescriptor> output_rd_;
};

TEST_P(ParamMemorySourceNodeTest, param_test) {
  for (const auto& [idx, action] : Enumerate(test_case_.actions)) {
    switch (action.type) {
      case ActionType::AddBatchToTable: {
        WriteBatch(action.times);
        break;
      }
      case ActionType::ExpectBatch: {
        EXPECT_TRUE(tester_->node()->HasBatchesRemaining());
        ExpectBatch(action.times, action.eow, action.eos);
        break;
      }
      case ActionType::CompactTable: {
        EXPECT_OK(cpu_table_->CompactHotToCold(arrow::default_memory_pool()));
        break;
      }
    }
  }

  EXPECT_EQ(test_case_.expect_batches_remaining, tester_->node()->HasBatchesRemaining());
  EXPECT_EQ(test_case_.expect_n_rows, tester_->node()->RowsProcessed());
  EXPECT_EQ(sizeof(int64_t) * test_case_.expect_n_rows, tester_->node()->BytesProcessed());
}

INSTANTIATE_TEST_SUITE_P(
    MemorySourceNodeTestSuite, ParamMemorySourceNodeTest,
    ::testing::ValuesIn(std::vector<MemorySourceTestCase>{
        {
            "empty_table",
            {},
            std::nullopt,
            std::nullopt,
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {},
                    true,
                    true,
                },
            },
            0,
            false,
        },
        {
            "full_table",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            std::nullopt,
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                    true,
                    true,
                },
            },
            5,
            false,
        },
        {
            "start_time_basic",
            {
                {1, 2, 3},
                {5, 6},
            },
            {3},
            std::nullopt,
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {3},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                    true,
                    true,
                },
            },
            3,
            false,
        },
        {
            "start_time_add_batch",
            {
                {1, 2, 3},
                {5, 6},
            },
            {5},
            std::nullopt,
            false,
            {
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                    true,
                    true,
                },
            },
            2,
            false,
        },
        {
            "start_time_future",
            {
                {1, 2, 3},
                {5, 6},
            },
            {7},
            std::nullopt,
            false,
            {
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {},
                    true,
                    true,
                },
            },
            0,
            false,
        },
        {
            "start_time_compaction",
            {
                {1, 2, 3},
                {5, 6},
            },
            {3},
            std::nullopt,
            false,
            {
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {3, 5},
                },
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {6},
                    true,
                    true,
                },
            },
            3,
            false,
        },
        {
            "start_time_basic_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            {3},
            std::nullopt,
            true,
            {
                {
                    ActionType::ExpectBatch,
                    {3},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                },
            },
            3,
            true,
        },
        {
            "start_time_add_batch_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            {5},
            std::nullopt,
            true,
            {
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {7, 8},
                },
            },
            4,
            true,
        },
        {
            "start_time_future_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            {7},
            std::nullopt,
            true,
            {
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {7, 8},
                },
            },
            2,
            true,
        },
        {
            "start_time_compaction_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            {3},
            std::nullopt,
            true,
            {
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {3, 5},
                },
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {6},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {7},
                },
                {
                    ActionType::ExpectBatch,
                    {8},
                },
            },
            5,
            true,
        },
        {
            "stop_time_basic",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {5},
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::ExpectBatch,
                    {5},
                    true,
                    true,
                },
            },
            4,
            false,
        },
        {
            "stop_time_add_batch",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {6},
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::AddBatchToTable,
                    {6, 6},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                    true,
                    true,
                },
                // When a stop time is specified without streaming, it will only consider batches in
                // the table at on Open() of the MemorySourceNode.
            },
            5,
            false,
        },
        {
            "stop_time_future",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {7},
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                    true,
                    true,
                },
            },
            5,
            false,
        },
        {
            "stop_time_compaction",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {5},
            false,
            {
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {1, 2},
                },
                {
                    ActionType::ExpectBatch,
                    {3, 5},
                    true,
                    true,
                },
            },
            4,
            false,
        },
        {
            "stop_time_basic_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {5},
            true,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::ExpectBatch,
                    {5},
                    true,
                    true,
                },
            },
            4,
            false,
        },
        {
            "stop_time_add_batch_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {5},
            true,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {5},
                    true,
                    true,
                },
            },
            4,
            false,
        },
        {
            "stop_time_future_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {7},
            true,
            {
                {
                    ActionType::ExpectBatch,
                    {1, 2, 3},
                },
                {
                    ActionType::ExpectBatch,
                    {5, 6},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 7, 8},
                },
                {
                    ActionType::ExpectBatch,
                    {7, 7},
                    true,
                    true,
                },
            },
            7,
            false,
        },
        {
            "stop_time_compaction_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            std::nullopt,
            {7},
            true,
            {
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {1, 2},
                },
                {
                    ActionType::ExpectBatch,
                    {3, 5},
                },
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {6},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 7},
                },
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {7},
                },
                {
                    ActionType::ExpectBatch,
                    {7},
                },
                {
                    ActionType::AddBatchToTable,
                    {8, 9},
                },
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {},
                    true,
                    true,
                },
            },
            7,
            false,
        },
        {
            "start_and_stop_time_basic",
            {
                {1, 2, 3},
                {5, 6},
            },
            {3},
            {5},
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {3},
                },
                {
                    ActionType::ExpectBatch,
                    {5},
                    true,
                    true,
                },
            },
            2,
            false,
        },
        {
            "start_and_stop_time_future",
            {
                {1, 2, 3},
                {5, 6},
            },
            {7},
            {10},
            false,
            {
                {
                    ActionType::ExpectBatch,
                    {},
                    true,
                    true,
                },
            },
            0,
            false,
        },
        {
            "start_and_stop_time_compaction",
            {
                {1, 2, 3},
                {5, 6},
            },
            {2},
            {6},
            false,
            {
                {
                    ActionType::CompactTable,
                },
                {
                    ActionType::ExpectBatch,
                    {2},
                },
                {
                    ActionType::ExpectBatch,
                    {3, 5},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 7},
                },
                {
                    ActionType::ExpectBatch,
                    {6},
                    true,
                    true,
                },
            },
            4,
            false,
        },
        {
            "start_and_stop_time_future_streaming",
            {
                {1, 2, 3},
                {5, 6},
            },
            {7},
            {10},
            true,
            {
                {
                    ActionType::ExpectBatch,
                    {},
                },
                {
                    ActionType::AddBatchToTable,
                    {7, 8, 9, 10},
                },
                {
                    ActionType::AddBatchToTable,
                    {10, 10},
                },
                {
                    ActionType::ExpectBatch,
                    {7, 8, 9, 10},
                },
                {
                    ActionType::ExpectBatch,
                    {10, 10},
                },
                {
                    ActionType::AddBatchToTable,
                    {11, 12},
                },
                {
                    ActionType::ExpectBatch,
                    {},
                    true,
                    true,
                },
            },
            6,
            false,
        },
    }),
    [](const ::testing::TestParamInfo<ParamMemorySourceNodeTest::ParamType>& info) {
      return info.param.name;
    });

}  // namespace exec
}  // namespace carnot
}  // namespace px
