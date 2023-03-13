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

#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service_mock.grpc.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service_mock.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/carnotpb/carnot_mock.grpc.pb.h"
#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/row_tuple.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/typespb/types.pb.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

// This function provides a mock generator for the result sink server that Carnot sends results to.
const ResultSinkStubGenerator MockResultSinkStubGenerator =
    [](const std::string&,
       const std::string&) -> std::unique_ptr<carnotpb::ResultSinkService::StubInterface> {
  return std::make_unique<carnotpb::MockResultSinkServiceStub>();
};

const MetricsStubGenerator MockMetricsStubGenerator = [](const std::string&, bool)
    -> std::unique_ptr<
        opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface> {
  return std::make_unique<opentelemetry::proto::collector::metrics::v1::MockMetricsServiceStub>();
};

const TraceStubGenerator MockTraceStubGenerator = [](const std::string&, bool)
    -> std::unique_ptr<opentelemetry::proto::collector::trace::v1::TraceService::StubInterface> {
  return std::make_unique<opentelemetry::proto::collector::trace::v1::MockTraceServiceStub>();
};

table_store::schema::RowBatch ConcatRowBatches(
    const std::vector<table_store::schema::RowBatch>& batches) {
  CHECK(batches.size());
  bool eos = false;
  bool eow = false;
  int64_t num_rows = 0;

  for (size_t i = 0; i < batches.size(); ++i) {
    if (i == batches.size() - 1) {
      eow = batches[i].eow();
      eos = batches[i].eos();
    } else {
      CHECK(!batches[i].eow());
      CHECK(!batches[i].eos());
      CHECK(batches[i].desc() == batches[batches.size() - 1].desc());
    }
    num_rows += batches[i].num_rows();
  }

  std::vector<std::unique_ptr<arrow::ArrayBuilder>> column_builders(batches[0].num_columns());
  for (size_t i = 0; i < batches[0].desc().size(); ++i) {
    column_builders[i] = MakeArrowBuilder(batches[0].desc().type(i), arrow::default_memory_pool());
    PX_CHECK_OK(column_builders[i]->Reserve(num_rows));
  }

  for (auto rb : batches) {
    for (auto col_idx = 0; col_idx < rb.num_columns(); ++col_idx) {
      auto input_col = rb.ColumnAt(col_idx).get();
      auto dt = rb.desc().type(col_idx);
      auto builder = column_builders[col_idx].get();
      for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
#define TYPE_CASE(_dt_)                             \
  PX_CHECK_OK(table_store::schema::CopyValue<_dt_>( \
      builder, types::GetValueFromArrowArray<_dt_>(input_col, row_idx)))
        PX_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
      }
    }
  }

  auto rb_ptr = table_store::schema::RowBatch::FromColumnBuilders(batches[0].desc(), eow, eos,
                                                                  &column_builders);
  return *(rb_ptr.ConsumeValueOrDie());
}

class CarnotTestUtils {
 public:
  CarnotTestUtils() = default;
  static std::shared_ptr<table_store::Table> TestTable() {
    table_store::schema::Relation rel({types::DataType::FLOAT64, types::DataType::INT64},
                                      {"col1", "col2"});
    auto table = table_store::Table::Create("test_table", rel);

    auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
    std::vector<types::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
    std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
    PX_CHECK_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
    PX_CHECK_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
    PX_CHECK_OK(table->WriteRowBatch(rb1));

    auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
    std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
    std::vector<types::Int64Value> col2_in2 = {5, 6};
    PX_CHECK_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
    PX_CHECK_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
    PX_CHECK_OK(table->WriteRowBatch(rb2));

    return table;
  }

  static std::shared_ptr<table_store::Table> TestDuration64Table() {
    table_store::schema::Relation rel({types::DataType::INT64}, {"col1"});
    auto table = table_store::Table::Create("test_table", rel);

    auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
    std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
    PX_CHECK_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
    PX_CHECK_OK(table->WriteRowBatch(rb1));

    return table;
  }

  static const std::vector<types::Time64NSValue> big_test_col1;
  static const std::vector<types::Float64Value> big_test_col2;
  static const std::vector<types::Int64Value> big_test_col3;
  static const std::vector<types::Int64Value> big_test_groups;
  static const std::vector<types::StringValue> big_test_strings;
  static const std::vector<std::pair<int64_t, int64_t>> split_idx;

  static std::shared_ptr<table_store::Table> BigTestTable() {
    table_store::schema::Relation rel(
        {types::DataType::TIME64NS, types::DataType::FLOAT64, types::DataType::INT64,
         types::DataType::INT64, types::DataType::STRING},
        {"time_", "col2", "col3", "num_groups", "string_groups"});

    auto table = table_store::Table::Create("test_table", rel);

    for (const auto& pair : split_idx) {
      auto rb = RowBatch(RowDescriptor(rel.col_types()), pair.second - pair.first);
      std::vector<types::Time64NSValue> col1_batch(big_test_col1.begin() + pair.first,
                                                   big_test_col1.begin() + pair.second);
      EXPECT_OK(rb.AddColumn(types::ToArrow(col1_batch, arrow::default_memory_pool())));

      std::vector<types::Float64Value> col2_batch(big_test_col2.begin() + pair.first,
                                                  big_test_col2.begin() + pair.second);
      EXPECT_OK(rb.AddColumn(types::ToArrow(col2_batch, arrow::default_memory_pool())));

      std::vector<types::Int64Value> col3_batch(big_test_col3.begin() + pair.first,
                                                big_test_col3.begin() + pair.second);
      EXPECT_OK(rb.AddColumn(types::ToArrow(col3_batch, arrow::default_memory_pool())));

      std::vector<types::Int64Value> col4_batch(big_test_groups.begin() + pair.first,
                                                big_test_groups.begin() + pair.second);
      EXPECT_OK(rb.AddColumn(types::ToArrow(col4_batch, arrow::default_memory_pool())));

      std::vector<types::StringValue> col5_batch(big_test_strings.begin() + pair.first,
                                                 big_test_strings.begin() + pair.second);
      EXPECT_OK(rb.AddColumn(types::ToArrow(col5_batch, arrow::default_memory_pool())));

      EXPECT_OK(table->WriteRowBatch(rb));
    }
    return table;
  }

  static std::shared_ptr<table_store::Table> ProcessStatsTable() {
    table_store::schema::Relation rel(
        {
            types::DataType::TIME64NS,
            types::DataType::UINT128,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
            types::DataType::INT64,
        },
        {
            "time_",
            "upid",
            "major_faults",
            "minor_faults",
            "cpu_utime_ns",
            "cpu_ktime_ns",
            "num_threads",
            "vsize_bytes",
            "rss_bytes",
            "rchar_bytes",
            "wchar_bytes",
            "read_bytes",
            "write_bytes",
        });
    auto table = table_store::Table::Create("process_table", rel);
    return table;
  }

  static std::shared_ptr<table_store::Table> HTTPEventsTable() {
    table_store::schema::Relation rel(
        {
            types::DataType::TIME64NS, types::DataType::UINT128, types::DataType::STRING,
            types::DataType::INT64,    types::DataType::INT64,   types::DataType::INT64,
            types::DataType::INT64,    types::DataType::INT64,   types::DataType::STRING,
            types::DataType::STRING,   types::DataType::STRING,  types::DataType::STRING,
            types::DataType::INT64,    types::DataType::STRING,  types::DataType::INT64,
            types::DataType::STRING,   types::DataType::STRING,  types::DataType::INT64,
            types::DataType::INT64,
        },
        {
            "time_",         "upid",          "remote_addr",    "remote_port",  "trace_role",
            "major_version", "minor_version", "content_type",   "req_headers",  "req_method",
            "req_path",      "req_body",      "req_body_size",  "resp_headers", "resp_status",
            "resp_message",  "resp_body",     "resp_body_size", "latency",
        });
    auto table = table_store::Table::Create("http_events_table", rel);
    return table;
  }
};

const std::vector<types::Time64NSValue> CarnotTestUtils::big_test_col1({1, 2, 3, 5, 6, 8, 9, 11});
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
  RowBatchBuilder(const table_store::schema::RowDescriptor& rd, int64_t size, bool eow_set,
                  bool eos_set) {
    rb_ = std::make_unique<table_store::schema::RowBatch>(rd, size);
    rb_->set_eow(eow_set);
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
    EXPECT_OK(rb_->AddColumn(std::move(col_arrow)));

    return *this;
  }

  /**
   * @return The rowbatch.
   */
  table_store::schema::RowBatch& get() { return *rb_; }

 private:
  std::unique_ptr<table_store::schema::RowBatch> rb_;
};

class FakePlanNode : public plan::Operator {
 public:
  explicit FakePlanNode(int64_t id) : Operator(id, planpb::OPERATOR_TYPE_UNKNOWN) {}
  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema&, const plan::PlanState&,
      const std::vector<int64_t>&) const override {
    // There are no outputs.
    return table_store::schema::Relation();
  }
  std::string DebugString() const override { return "FakePlanNode"; }
};

/*
 * Test wrapper for testing execution nodes.
 * Example usage:
 *   auto node_tester = exec::ExecNodeTester<MapNode, plan::MapOperator>();
 *   node_tester.ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
 *                      .AddColumn<udf::Int64Value>({1, 2, 3})
 *                      .AddColumn<udf::Int64Value>({1, 4, 6})
 *                      .get(), 5)
 *     .ExpectRowBatch(
 *         RowBatchBuilder(output_rd, 3, false, false).AddColumn<udf::Int64Value>({2, 6, 9}).get())
 *     .Close();
 */
template <typename TExecNode, typename TPlanNode>
class ExecNodeTester {
 public:
  template <typename... Args>
  ExecNodeTester(const plan::Operator& plan_node,
                 const table_store::schema::RowDescriptor& output_descriptor,
                 std::vector<table_store::schema::RowDescriptor> input_descriptors,
                 ExecState* exec_state, Args... exec_node_args)
      : exec_state_(exec_state) {
    exec_node_ = std::make_unique<TExecNode>(exec_node_args...);
    const auto* casted_plan_node = static_cast<const TPlanNode*>(&plan_node);

    // Set the exec state to use a fake source of ID 1 during the course of the test.
    exec_state_->SetCurrentSource(1);

    // copy the plan node to local object;
    plan_node_ = std::make_unique<TPlanNode>(*casted_plan_node);

    if (!exec_node_->IsSink()) {
      exec_node_->AddChild(&mock_child_, 0);
    }

    EXPECT_OK(exec_node_->Init(*plan_node_, output_descriptor, input_descriptors));
    EXPECT_OK(exec_node_->Prepare(exec_state_));
    EXPECT_OK(exec_node_->Open(exec_state_));
    FakePlanNode fake_plan(123);
    EXPECT_CALL(mock_child_, InitImpl(::testing::_));
    EXPECT_CALL(mock_child_, PrepareImpl(::testing::_));
    EXPECT_CALL(mock_child_, OpenImpl(::testing::_));
    // Setting up mock child.
    EXPECT_OK(
        mock_child_.Init(fake_plan, table_store::schema::RowDescriptor({}), {output_descriptor}));
    EXPECT_OK(mock_child_.Prepare(exec_state_));
    EXPECT_OK(mock_child_.Open(exec_state_));
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
    auto check_result_batch = [&](ExecState*, const table_store::schema::RowBatch& child_rb,
                                  int64_t) {
      current_row_batches_.push(std::make_unique<table_store::schema::RowBatch>(child_rb));
    };

    EXPECT_CALL(mock_child_, ConsumeNextImpl(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(::testing::Invoke(check_result_batch),
                                   ::testing::Return(Status::OK())))
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
  ExecNodeTester& ConsumeNextShouldFail(const table_store::schema::RowBatch& rb, int64_t parent_id,
                                        Status error) {
    EXPECT_CALL(mock_child_, ConsumeNextImpl(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(error));

    auto retval = exec_node_->ConsumeNext(exec_state_, rb, parent_id);
    EXPECT_NOT_OK(retval);

    return *this;
  }

  /**
   * Calls ConsumeNext on the execution node.
   * @param rb The input rowbatch to ConsumeNext.
   * @param child_called Whether the mock child's ConsumeNext should be called.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& ConsumeNext(const table_store::schema::RowBatch& rb, int64_t parent_id,
                              size_t child_called_times = 1) {
    auto check_result_batch = [&](ExecState*, const table_store::schema::RowBatch& child_rb,
                                  int64_t) {
      current_row_batches_.push(std::make_unique<table_store::schema::RowBatch>(child_rb));
    };

    if (child_called_times > 0) {
      EXPECT_CALL(mock_child_, ConsumeNextImpl(::testing::_, ::testing::_, ::testing::_))
          .Times(child_called_times)
          .WillRepeatedly(::testing::DoAll(::testing::Invoke(check_result_batch),
                                           ::testing::Return(Status::OK())));
    }
    auto s = exec_node_->ConsumeNext(exec_state_, rb, parent_id);
    EXPECT_OK(s) << s.msg();

    return *this;
  }

  /**
   * Checks that the row batch matches the last rowbatch output by ConsumeNext/GenerateNext.
   * @param expected_rb Row batch that should match the last rowbatch output by
   * ConsumeNext/GenerateNext.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& ExpectRowBatch(const table_store::schema::RowBatch& expected_rb,
                                 bool ordered = true, int64_t time_column_idx = -1) {
    if (ordered) {
      DCHECK(current_row_batches_.size());
      ValidateRowBatch(expected_rb, *current_row_batches_.front().get());
    } else {
      ValidateUnorderedRowBatch(expected_rb, *current_row_batches_.front().get());
    }
    if (time_column_idx > -1) {
      ValidateTimeOrder(*current_row_batches_.front().get(), time_column_idx);
    }
    current_row_batches_.pop();

    return *this;
  }

  /**
   * Checks that the row batch matches the last rowbatch output by ConsumeNext/GenerateNext.
   * @param expected_rb Row batch that should match the last rowbatch output by
   * ConsumeNext/GenerateNext.
   * @return the ExecNodeTester, to allow for chaining.
   */
  ExecNodeTester& ExpectRowBatchesData(const table_store::schema::RowBatch& expected_rb,
                                       int64_t num_batches, int64_t time_column_idx = -1) {
    std::vector<table_store::schema::RowBatch> batches;
    for (auto i = 0; i < num_batches; ++i) {
      batches.push_back(*current_row_batches_.front().get());
      current_row_batches_.pop();
    }
    auto actual_rb = ConcatRowBatches(batches);

    ValidateUnorderedRowBatch(expected_rb, actual_rb);

    if (time_column_idx > -1) {
      ValidateTimeOrder(actual_rb, time_column_idx);
    }

    return *this;
  }

 private:
  void ValidateRowBatch(const table_store::schema::RowBatch& expected_rb,
                        const table_store::schema::RowBatch& actual_rb) {
    EXPECT_EQ(actual_rb.num_rows(), expected_rb.num_rows());
    EXPECT_EQ(actual_rb.num_columns(), expected_rb.num_columns());
    for (size_t i = 0; i < actual_rb.desc().size(); i++) {
      EXPECT_EQ(actual_rb.desc().type(i), expected_rb.desc().type(i));
    }

    for (int64_t i = 0; i < actual_rb.num_columns(); i++) {
      EXPECT_TRUE(expected_rb.ColumnAt(i)->Equals(actual_rb.ColumnAt(i)));
    }
    EXPECT_EQ(actual_rb.eow(), expected_rb.eow());
    EXPECT_EQ(actual_rb.eos(), expected_rb.eos());
  }

  template <px::types::DataType DT>
  void SetRowTupleValues(RowTuple* expected_rt, RowTuple* actual_rt, arrow::Array* expected_arr,
                         arrow::Array* actual_arr, int64_t col, int64_t row) {
    using ValueType = typename px::types::DataTypeTraits<DT>::value_type;

    expected_rt->SetValue(col, ValueType(types::GetValueFromArrowArray<DT>(expected_arr, row)));
    actual_rt->SetValue(col, ValueType(types::GetValueFromArrowArray<DT>(actual_arr, row)));
  }

  void ValidateUnorderedRowBatch(const table_store::schema::RowBatch& expected_rb,
                                 const table_store::schema::RowBatch& actual_rb) {
    EXPECT_EQ(actual_rb.num_rows(), expected_rb.num_rows());
    EXPECT_EQ(actual_rb.num_columns(), expected_rb.num_columns());
    EXPECT_EQ(actual_rb.eow(), expected_rb.eow());
    EXPECT_EQ(actual_rb.eos(), expected_rb.eos());

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
        PX_SWITCH_FOREACH_DATATYPE(expected_rb.desc().types()[col], TYPE_CASE);
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

  void ValidateTimeOrder(const table_store::schema::RowBatch& rb, size_t time_column_idx) {
    types::Time64NSValue prev_val;
    types::Time64NSValue cur_val;
    auto time_col = rb.ColumnAt(time_column_idx).get();
    for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
      prev_val = cur_val;
      cur_val = types::GetValueFromArrowArray<types::TIME64NS>(time_col, row_idx);
      if (row_idx == 0) {
        continue;
      }
      EXPECT_GE(cur_val, prev_val);
    }
  }

  MockExecNode mock_child_;
  std::unique_ptr<TExecNode> exec_node_;
  std::unique_ptr<TPlanNode> plan_node_;
  ExecState* exec_state_;
  std::queue<std::unique_ptr<table_store::schema::RowBatch>> current_row_batches_;
};
}  // namespace exec
}  // namespace carnot
}  // namespace px
