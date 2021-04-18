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

#include <arrow/array/builder_base.h>
#include <cstddef>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/row_tuple.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/common/memory/memory.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

constexpr size_t kDefaultJoinRowBatchSize = 1024;

class EquijoinNode : public ProcessingNode {
  enum class JoinInputTable { kLeftTable, kRightTable };

  struct TableSpec {
    bool emit_unmatched_rows;
    // Indices of the input columns for the keys.
    std::vector<int64_t> key_indices;
    // Indices of the input columns that are outputted by the build/probe.
    std::vector<int64_t> input_col_indices;
    std::vector<types::DataType> input_col_types;
    // For each of the values in input_col_indices, the index in the output row batch
    // that column should be written to.
    std::vector<int64_t> output_col_indices;
  };

 public:
  EquijoinNode() = default;
  virtual ~EquijoinNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  Status InitializeColumnBuilders();
  bool IsProbeTable(size_t parent_index);
  Status FlushChunkedRows(ExecState* exec_state);
  Status ExtractJoinKeysForBatch(const table_store::schema::RowBatch& rb, bool is_probe);
  Status HashRowBatch(const table_store::schema::RowBatch& rb);

  Status DoProbe(ExecState* exec_state, const table_store::schema::RowBatch& rb);
  Status MatchBuildValuesAndFlush(ExecState* exec_state,
                                  std::vector<types::SharedColumnWrapper>* wrapper,
                                  std::shared_ptr<table_store::schema::RowBatch> probe_rb,
                                  int64_t probe_rb_row_idx, int64_t matching_bb_rows);
  Status EmitUnmatchedBuildRows(ExecState* exec_state);
  Status NextOutputBatch(ExecState* exec_state);
  Status ConsumeBuildBatch(ExecState* exec_state, const table_store::schema::RowBatch& rb);
  Status ConsumeProbeBatch(ExecState* exec_state, const table_store::schema::RowBatch& rb);

  bool build_eos_ = false;
  bool probe_eos_ = false;
  // Note whether the left or the right table is the probe table.
  JoinInputTable probe_table_;
  // output_rows_per_batch is only used in the ordered case, because in the unordered case,
  // we just maintain the original row count to avoid copying the data.
  int64_t output_rows_per_batch_;

  // Specification for the join for each of the input tables.
  TableSpec build_spec_;
  TableSpec probe_spec_;

  std::vector<types::DataType> key_data_types_;

  // Example of the above specs:
  // For input table A (build) which has [key_A_1, output_col_0, key_A_0/output_col_2]
  // and input table B (probe) which has [key_B_0, output_col_3, key_B_1/output_col_1]
  // build_spec_: {key_indices: [2, 0], input_col_indices: [1, 2], output_col_indices: [0, 2]}
  // probe_spec_: {key_indices: [0, 2], input_col_indices: [1, 2], output_col_indices: [3, 1]}
  // produces table [output_col_0, output_col_1(key_B_1), output_col_2(key_A_0), output_col_3]

  // This struct represents a chunk of output rows that are queued to be written
  // to the column builders.
  struct OutputChunk {
    std::shared_ptr<table_store::schema::RowBatch> rb;
    std::vector<types::SharedColumnWrapper>* wrappers_ptr;
    int64_t num_rows;
    int64_t bb_row_idx;
    int64_t probe_row_idx;
  };

  int64_t queued_rows_ = 0;
  std::vector<OutputChunk> chunks_;

  // Memory/column building members
  // If the build stage isn't complete, we need to buffer the probe batches.
  std::queue<table_store::schema::RowBatch> probe_batches_;
  // Column builders will flush a batch once they hit output_rows_per_batch_ rows.
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> column_builders_;
  // Manages the RowTuples containing the keys for the join.
  ObjectPool key_values_pool_{"equijoin_kv_pool"};
  ObjectPool column_values_pool_{"equijoin_col_vals_pool"};

  // Chunk of data to use when extracting join keys.
  std::vector<RowTuple*> join_keys_chunk_;
  // Chunk of data to use when performing the build stage of the join.
  std::vector<std::vector<types::SharedColumnWrapper>*> build_wrappers_chunk_;

  // Chunk of data to use when performing the probe stage of the join.
  // This will store build table data from `build_buffer_`.
  std::vector<std::vector<types::SharedColumnWrapper>*> probe_wrappers_chunk_;
  AbslRowTupleHashMap<std::vector<types::SharedColumnWrapper>*> build_buffer_;
  // Store the number of rows that match a given set of keys for the build buffer.
  // This is necessary to store in addition to the values in `build_buffer_` in
  // the event that no columns from the build side are emitted.
  AbslRowTupleHashMap<int64_t> build_buffer_rows_;

  // For joins where the build_buffer_ needs to emit any non-probed rows at the end of the join,
  // keep track of which ones they were.
  AbslRowTupleHashSet probed_keys_;

  // Handle on the most recent RowBatch (in case it's the final one).
  std::unique_ptr<table_store::schema::RowBatch> pending_output_batch_;

  std::unique_ptr<plan::JoinOperator> plan_node_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
