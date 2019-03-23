#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/row_tuple.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/schema/row_descriptor.h"
#include "src/carnot/schema/table.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

class CarnotTestUtils {
 public:
  CarnotTestUtils() = default;
  static std::shared_ptr<schema::Table> TestTable() {
    schema::Relation rel({types::DataType::FLOAT64, types::DataType::INT64}, {"col1", "col2"});
    auto table = std::make_shared<schema::Table>(rel);

    auto col1 = table->GetColumn(0);
    std::vector<types::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
    std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
    PL_CHECK_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
    PL_CHECK_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

    auto col2 = table->GetColumn(1);
    std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
    std::vector<types::Int64Value> col2_in2 = {5, 6};
    PL_CHECK_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
    PL_CHECK_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

    return table;
  }

  static const std::vector<types::Int64Value> big_test_col1;
  static const std::vector<types::Float64Value> big_test_col2;
  static const std::vector<types::Int64Value> big_test_col3;
  static const std::vector<types::Int64Value> big_test_groups;
  static const std::vector<types::StringValue> big_test_strings;
  static const std::vector<std::pair<int64_t, int64_t>> split_idx;

  static std::shared_ptr<schema::Table> BigTestTable() {
    schema::Relation rel({types::DataType::TIME64NS, types::DataType::FLOAT64,
                          types::DataType::INT64, types::DataType::INT64, types::DataType::STRING},
                         {"time_", "col2", "col3", "num_groups", "string_groups"});

    auto table = std::make_shared<schema::Table>(rel);

    auto col1 = table->GetColumn(0);
    auto col2 = table->GetColumn(1);
    auto col3 = table->GetColumn(2);
    auto col4 = table->GetColumn(3);
    auto col5 = table->GetColumn(4);

    for (const auto& pair : split_idx) {
      std::vector<types::Int64Value> col1_batch(big_test_col1.begin() + pair.first,
                                                big_test_col1.begin() + pair.second);
      EXPECT_OK(col1->AddBatch(types::ToArrow(col1_batch, arrow::default_memory_pool())));

      std::vector<types::Float64Value> col2_batch(big_test_col2.begin() + pair.first,
                                                  big_test_col2.begin() + pair.second);
      EXPECT_OK(col2->AddBatch(types::ToArrow(col2_batch, arrow::default_memory_pool())));

      std::vector<types::Int64Value> col3_batch(big_test_col3.begin() + pair.first,
                                                big_test_col3.begin() + pair.second);
      EXPECT_OK(col3->AddBatch(types::ToArrow(col3_batch, arrow::default_memory_pool())));

      std::vector<types::Int64Value> col4_batch(big_test_groups.begin() + pair.first,
                                                big_test_groups.begin() + pair.second);
      EXPECT_OK(col4->AddBatch(types::ToArrow(col4_batch, arrow::default_memory_pool())));

      std::vector<types::StringValue> col5_batch(big_test_strings.begin() + pair.first,
                                                 big_test_strings.begin() + pair.second);
      EXPECT_OK(col5->AddBatch(types::ToArrow(col5_batch, arrow::default_memory_pool())));
    }
    return table;
  }
};

const std::vector<types::Int64Value> CarnotTestUtils::big_test_col1({1, 2, 3, 5, 6, 8, 9, 11});
const std::vector<types::Float64Value> CarnotTestUtils::big_test_col2({0.5, 1.2, 5.3, 0.1, 5.1, 5.2,
                                                                       0.1, 7.3});
const std::vector<types::Int64Value> CarnotTestUtils::big_test_col3({6, 2, 12, 5, 60, 56, 12, 13});
const std::vector<types::Int64Value> CarnotTestUtils::big_test_groups({1, 1, 3, 1, 2, 2, 3, 2});
const std::vector<types::StringValue> CarnotTestUtils::big_test_strings({"sum", "mean", "sum",
                                                                         "mean", "sum", "mean",
                                                                         "sum", "mean"});
const std::vector<std::pair<int64_t, int64_t>> CarnotTestUtils::split_idx({{0, 3}, {3, 5}, {5, 8}});

/**
 * Util for creating row batches.
 */
class RowBatchBuilder {
 public:
  RowBatchBuilder(const schema::RowDescriptor& rd, int64_t size, bool eos_set) {
    rb_ = std::make_unique<schema::RowBatch>(rd, size);
    rb_->set_eos(eos_set);
  }

  /**
   * Add a column to the rowbatch.
   * @tparam TUDF The type of column.
   * @param col The column to add to the rowbatch.
   * @return the RowBatchBuilder, to allow for chaining.
   */
  template <typename TUDF>
  RowBatchBuilder& AddColumn(std::vector<TUDF> col) {
    auto col_arrow = types::ToArrow(col, arrow::default_memory_pool());
    EXPECT_TRUE(rb_->AddColumn(std::move(col_arrow)).ok());

    return *this;
  }

  /**
   * @return The rowbatch.
   */
  schema::RowBatch& get() { return *rb_; }

 private:
  std::unique_ptr<schema::RowBatch> rb_;
};

/*
 * Test wrapper for testing execution nodes.
 * Example usage:
 *   auto node_tester = exec::ExecNodeTester<MapNode, plan::MapOperator>();
 *   node_tester.ConsumeNext(RowBatchBuilder(input_rd, 3, true)
 *                      .AddColumn<udf::Int64Value>({1, 2, 3})
 *                      .AddColumn<udf::Int64Value>({1, 4, 6})
 *                      .get())
 *     .ExpectRowBatch(
 *         RowBatchBuilder(output_rd, 3, false).AddColumn<udf::Int64Value>({2, 6, 9}).get())
 *     .Close();
 */
template <typename TExecNode, typename TPlanNode>
class ExecNodeTester {
 public:
  ExecNodeTester(const plan::Operator& plan_node, const schema::RowDescriptor& output_descriptor,
                 std::vector<schema::RowDescriptor> input_descriptors, ExecState* exec_state)
      : output_descriptor_(output_descriptor),
        input_descriptors_(input_descriptors),
        exec_state_(exec_state) {
    exec_node_ = std::make_unique<TExecNode>();
    const auto* casted_plan_node = static_cast<const TPlanNode*>(&plan_node);
    // copy the plan node to local object;
    plan_node_ = std::make_unique<TPlanNode>(*casted_plan_node);

    if (!exec_node_->IsSink()) {
      exec_node_->AddChild(&mock_child_);
    }

    EXPECT_OK(exec_node_->Init(*plan_node_, output_descriptor_, input_descriptors_));
    EXPECT_OK(exec_node_->Prepare(exec_state_));
    EXPECT_OK(exec_node_->Open(exec_state_));
  }

  /**
   * @return the execution node.
   */
  TExecNode* node() { return exec_node_.get(); }

  /**
   * Calls Close on the execution node.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& Close() {
    EXPECT_OK(exec_node_->Close(exec_state_));
    return *this;
  }

  /**
   * Calls GenerateNextResult on the exec node.
   * This should only be called for source nodes.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& GenerateNextResult() {
    auto check_result_batch = [&](ExecState*, const schema::RowBatch& child_rb) {
      current_rb_ = std::make_unique<schema::RowBatch>(child_rb);
    };

    EXPECT_CALL(mock_child_, ConsumeNextImpl(testing::_, testing::_))
        .Times(1)
        .WillOnce(
            testing::DoAll(testing::Invoke(check_result_batch), testing::Return(Status::OK())))
        .RetiresOnSaturation();
    EXPECT_OK(exec_node_->GenerateNext(exec_state_));

    return *this;
  }

  /**
   * Calls ConsumeNext on the execution node, and check that calling ConsumeNext should fail.
   * @param rb The input rowbatch to ConsumeNext.
   * @param error The expected error that ConsumeNext should fail with.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& ConsumeNextShouldFail(const schema::RowBatch& rb, Status error) {
    EXPECT_CALL(mock_child_, ConsumeNextImpl(testing::_, testing::_))
        .Times(1)
        .WillRepeatedly(testing::Return(error));

    auto retval = exec_node_->ConsumeNext(exec_state_, rb);
    EXPECT_FALSE(retval.ok());

    return *this;
  }

  /**
   * Calls ConsumeNext on the execution node.
   * @param rb The input rowbatch to ConsumeNext.
   * @param child_called Whether the mock child's ConsumeNext should be called.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& ConsumeNext(const schema::RowBatch& rb, bool child_called = true) {
    auto check_result_batch = [&](ExecState*, const schema::RowBatch& child_rb) {
      current_rb_ = std::make_unique<schema::RowBatch>(child_rb);
    };

    if (child_called) {
      EXPECT_CALL(mock_child_, ConsumeNextImpl(testing::_, testing::_))
          .Times(1)
          .WillOnce(
              testing::DoAll(testing::Invoke(check_result_batch), testing::Return(Status::OK())));
    }
    EXPECT_OK(exec_node_->ConsumeNext(exec_state_, rb));

    return *this;
  }

  /**
   * Checks that the row batch matches the last rowbatch output by ConsumeNext/GenerateNext.
   * @param expected_rb Row batch that should match the last rowbatch output by
   * ConsumeNext/GenerateNext.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& ExpectRowBatch(const schema::RowBatch& expected_rb, bool ordered = true) {
    if (ordered) {
      ValidateRowBatch(expected_rb, *current_rb_.get());
    } else {
      ValidateUnorderedRowBatch(expected_rb, *current_rb_.get());
    }

    return *this;
  }

 private:
  void ValidateRowBatch(const schema::RowBatch& expected_rb, const schema::RowBatch& actual_rb) {
    EXPECT_EQ(actual_rb.num_rows(), expected_rb.num_rows());
    EXPECT_EQ(actual_rb.num_columns(), expected_rb.num_columns());
    for (size_t i = 0; i < actual_rb.desc().size(); i++) {
      EXPECT_EQ(actual_rb.desc().type(i), expected_rb.desc().type(i));
    }

    for (int64_t i = 0; i < actual_rb.num_columns(); i++) {
      EXPECT_TRUE(expected_rb.ColumnAt(i)->Equals(actual_rb.ColumnAt(i)));
    }
    EXPECT_EQ(actual_rb.eos(), expected_rb.eos());
  }

  template <pl::types::DataType DT>
  void SetRowTupleValues(RowTuple* expected_rt, RowTuple* actual_rt, arrow::Array* expected_arr,
                         arrow::Array* actual_arr, int64_t col, int64_t row) {
    using ValueType = typename pl::types::DataTypeTraits<DT>::value_type;

    expected_rt->SetValue(col, ValueType(types::GetValueFromArrowArray<DT>(expected_arr, row)));
    actual_rt->SetValue(col, ValueType(types::GetValueFromArrowArray<DT>(actual_arr, row)));
  }

  void ValidateUnorderedRowBatch(const schema::RowBatch& expected_rb,
                                 const schema::RowBatch& actual_rb) {
    EXPECT_EQ(actual_rb.num_rows(), expected_rb.num_rows());
    EXPECT_EQ(actual_rb.num_columns(), expected_rb.num_columns());

    // Convert row batches to hashable row tuples.
    std::vector<std::unique_ptr<RowTuple>> expected_rt;
    std::vector<std::unique_ptr<RowTuple>> actual_rt;

    const auto& expected_rb_types = expected_rb.desc().types();
    for (int64_t i = 0; i < actual_rb.num_rows(); i++) {
      auto expected_tuple = std::make_unique<RowTuple>(&expected_rb_types);
      auto actual_tuple = std::make_unique<RowTuple>(&expected_rb_types);
      expected_rt.push_back(std::move(expected_tuple));
      actual_rt.push_back(std::move(actual_tuple));
    }

    for (int64_t col = 0; col < actual_rb.num_columns(); col++) {
      for (int64_t row = 0; row < actual_rb.num_rows(); row++) {
#define TYPE_CASE(_dt_)                                                                        \
  SetRowTupleValues<_dt_>(expected_rt[row].get(), actual_rt[row].get(),                        \
                          expected_rb.ColumnAt(col).get(), actual_rb.ColumnAt(col).get(), col, \
                          row);
        PL_SWITCH_FOREACH_DATATYPE(expected_rb.desc().types()[col], TYPE_CASE);
#undef TYPE_CASE
      }
    }

    std::vector<size_t> expected_hashes;
    std::vector<size_t> actual_hashes;
    for (int64_t i = 0; i < actual_rb.num_rows(); i++) {
      expected_hashes.push_back(expected_rt[i]->Hash());
      actual_hashes.push_back(actual_rt[i]->Hash());
    }

    std::sort(expected_hashes.begin(), expected_hashes.end());
    std::sort(actual_hashes.begin(), actual_hashes.end());

    EXPECT_THAT(expected_hashes, actual_hashes);
  }

  MockExecNode mock_child_;
  std::unique_ptr<TExecNode> exec_node_;
  std::unique_ptr<TPlanNode> plan_node_;
  schema::RowDescriptor output_descriptor_;
  std::vector<schema::RowDescriptor> input_descriptors_;
  ExecState* exec_state_;
  std::unique_ptr<schema::RowBatch> current_rb_;
};
}  // namespace exec
}  // namespace carnot
}  // namespace pl
