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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table/internal/record_or_row_batch.h"

namespace px {
namespace table_store {
namespace internal {

class RecordOrRowBatchParamTest : public ::testing::TestWithParam<std::vector<bool>> {
 protected:
  void SetUp() override {
    rel_ = std::make_unique<schema::Relation>(
        std::vector<types::DataType>{types::DataType::TIME64NS, types::DataType::BOOLEAN,
                                     types::DataType::STRING},
        std::vector<std::string>{"col0", "col1", "col2"});

    idx_ = 0;
  }

  using ColSizes = std::vector<size_t>;

  // Get the next param from the vector of bool's, wrap around the vector if needed.
  bool GetNextParamCircular() {
    bool next = GetParam()[idx_];
    idx_++;
    if (idx_ == GetParam().size()) {
      idx_ = 0;
    }
    return next;
  }

  schema::RowBatch MakeRowBatch(const std::vector<types::Time64NSValue>& times,
                                const std::vector<types::BoolValue>& bools,
                                const std::vector<types::StringValue>& strings) {
    schema::RowBatch rb(schema::RowDescriptor(rel_->col_types()), times.size());
    PX_CHECK_OK(rb.AddColumn(types::ToArrow(times, arrow::default_memory_pool())));
    PX_CHECK_OK(rb.AddColumn(types::ToArrow(bools, arrow::default_memory_pool())));
    PX_CHECK_OK(rb.AddColumn(types::ToArrow(strings, arrow::default_memory_pool())));
    return rb;
  }

  std::unique_ptr<RecordBatchWithCache> MakeRecordBatch(
      const std::vector<types::Time64NSValue>& times, const std::vector<types::BoolValue>& bools,
      const std::vector<types::StringValue>& strings) {
    auto times_wrapper = types::ColumnWrapper::FromArrow(
        types::DataType::TIME64NS, types::ToArrow(times, arrow::default_memory_pool()));
    auto bools_wrapper =
        types::ColumnWrapper::FromArrow(types::ToArrow(bools, arrow::default_memory_pool()));
    auto strings_wrapper =
        types::ColumnWrapper::FromArrow(types::ToArrow(strings, arrow::default_memory_pool()));
    auto record_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
    record_batch->push_back(times_wrapper);
    record_batch->push_back(bools_wrapper);
    record_batch->push_back(strings_wrapper);

    auto rb_w_cache = std::make_unique<RecordBatchWithCache>();
    rb_w_cache->record_batch = std::move(record_batch);
    size_t num_cols = 3;
    rb_w_cache->cache_validity = std::vector<bool>(num_cols, false);
    rb_w_cache->arrow_cache = std::vector<ArrowArrayPtr>(num_cols, nullptr);
    return rb_w_cache;
  }

  std::pair<std::unique_ptr<RecordOrRowBatch>, ColSizes> MakeRecordOrRowBatch(
      std::vector<types::Time64NSValue> times, std::vector<types::BoolValue> bools,
      std::vector<types::StringValue> strings) {
    bool use_row_batch = GetNextParamCircular();

    std::unique_ptr<RecordOrRowBatch> record_or_row_batch;
    if (use_row_batch) {
      record_or_row_batch = std::make_unique<RecordOrRowBatch>(MakeRowBatch(times, bools, strings));
    } else {
      record_or_row_batch =
          std::make_unique<RecordOrRowBatch>(std::move(*MakeRecordBatch(times, bools, strings)));
    }

    ColSizes rb_col_sizes;
    rb_col_sizes.push_back(0);
    rb_col_sizes.push_back(0);
    auto total_str_bytes =
        std::accumulate(strings.begin(), strings.end(), 0UL,
                        [](size_t sum, const auto& s) { return s.size() + sum; });
    rb_col_sizes.push_back(total_str_bytes);
    return std::make_pair(std::move(record_or_row_batch), std::move(rb_col_sizes));
  }

  std::unique_ptr<schema::Relation> rel_;
  size_t idx_;
};

// Test both only RowBatch's, only RecordBatchWithCache's and a mix between the two.
#define INSTANTIATE_RECORD_OR_ROW_BATCH_TESTSUITE(name, fixture, include_mixed)           \
  INSTANTIATE_TEST_SUITE_P(name, fixture, ::testing::ValuesIn(({                          \
                             std::vector<std::vector<bool>> values;                       \
                             values.push_back({true});                                    \
                             values.push_back({false});                                   \
                             if ((include_mixed)) {                                       \
                               values.push_back({true, false});                           \
                             }                                                            \
                             values;                                                      \
                           })),                                                           \
                           [](const ::testing::TestParamInfo<fixture::ParamType>& info) { \
                             std::vector<bool> use_row_batch_vals = info.param;           \
                             if (use_row_batch_vals.size() > 1) {                         \
                               return "Mixed";                                            \
                             } else if (use_row_batch_vals[0]) {                          \
                               return "RowBatch";                                         \
                             } else {                                                     \
                               return "RecordBatchWithCache";                             \
                             }                                                            \
                           });
}  // namespace internal
}  // namespace table_store
}  // namespace px
