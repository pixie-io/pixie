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
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
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

TEST_F(MemorySourceNodeTest, infinite_stream) {
  auto op_proto = planpb::testutils::CreateTestStreamingSource1PB();
  std::unique_ptr<plan::Operator> plan_node = plan::MemorySourceOperator::FromProto(op_proto, 1);
  EXPECT_TRUE(static_cast<plan::MemorySourceOperator*>(plan_node.get())->infinite_stream());
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

}  // namespace exec
}  // namespace carnot
}  // namespace px
