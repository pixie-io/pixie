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

#include <arrow/array.h>
#include <arrow/array/builder_base.h>
#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

constexpr size_t kDefaultUnionRowBatchSize = 1024;
constexpr size_t kDefaultDataFlushTimeoutMillis = 1000;

// This node presumes that input streams will always come in ordered by time
// when there is a time column.
class UnionNode : public ProcessingNode {
 public:
  UnionNode() = default;
  virtual ~UnionNode() = default;

  // For the time ordered case: A MergeRow represents a row in an input row batch.
  struct MergeRow {
    size_t parent_index;
    size_t row_batch_id;
    size_t row_number;
    types::Time64NSValue time;
    bool end_of_row_batch;
  };

  void disable_data_flush_timeout() { enable_data_flush_timeout_ = false; }
  void set_data_flush_timeout(const std::chrono::milliseconds& data_flush_timeout) {
    enable_data_flush_timeout_ = true;
    data_flush_timeout_ = data_flush_timeout;
  }

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  bool InputsComplete();
  std::shared_ptr<arrow::Array> GetInputColumn(const table_store::schema::RowBatch& rb,
                                               size_t parent_index, size_t column_index);
  Status ConsumeNextUnordered(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                              size_t parent_index);
  Status ConsumeNextOrdered(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                            size_t parent_index);

  size_t num_parents_;
  std::vector<bool> flushed_parent_eoses_;

  std::unique_ptr<plan::UnionOperator> plan_node_;

  // The items below are all for the time-ordered case.

  void CacheNextRowBatch(size_t parent);
  Status InitializeColumnBuilders();
  types::Time64NSValue GetTimeAtParentCursor(size_t parent_index) const;
  Status AppendRow(size_t parent);
  Status OptionallyFlushRowBatchIfMaxRowsOrEOS(ExecState* exec_state);
  Status OptionallyFlushRowBatchIfTimeout(ExecState* exec_state);
  Status FlushBatch(ExecState* exec_state);
  Status MergeData(ExecState* exec_state);

  // output_rows_per_batch is only used in the ordered case, because in the unordered case,
  // we just maintain the original row count to avoid copying the data.
  size_t output_rows_per_batch_;

  // Column builders will flush a batch once they hit output_rows_per_batch_ rows.
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> column_builders_;

  // Hold onto the input row batches for every parent until we copy all of their data.
  std::vector<std::vector<table_store::schema::RowBatch>> parent_row_batches_;
  // Keep track of where we are in the stream for each parent.
  // The row is always relative to the 'top' row batch that we have for each parent.
  std::vector<size_t> row_cursors_;
  // Cache current working time and data columns for performance reasons.
  std::vector<arrow::Array*> time_columns_;
  std::vector<std::vector<arrow::Array*>> data_columns_;

  bool enable_data_flush_timeout_ = true;
  // When enable_data_flush_timeout_ is set to true, use this time to decide if we should
  // flush data to consumers before the output row batch reaches a certain size.
  std::chrono::milliseconds data_flush_timeout_ =
      std::chrono::milliseconds(kDefaultDataFlushTimeoutMillis);
  std::chrono::time_point<std::chrono::system_clock> last_data_flush_time_ =
      std::chrono::system_clock::now();
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
