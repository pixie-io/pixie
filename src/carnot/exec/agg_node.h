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
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/exec/row_tuple.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/udf.h"
#include "src/carnot/udf/udf_definition.h"
#include "src/common/base/base.h"
#include "src/common/memory/memory.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/hash_utils.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;

struct UDAInfo {
  UDAInfo(std::unique_ptr<udf::UDA> uda_inst, udf::UDADefinition* def_ptr)
      : uda(std::move(uda_inst)), def(def_ptr) {}
  std::unique_ptr<udf::UDA> uda;
  // unowned pointer to the definition;
  udf::UDADefinition* def = nullptr;
};

struct AggHashValue {
  std::vector<UDAInfo> udas;
  std::vector<types::SharedColumnWrapper> agg_cols;
};

struct GroupArgs {
  explicit GroupArgs(RowTuple* rt) : rt(rt), av(nullptr) {}
  RowTuple* rt;
  AggHashValue* av;
};

class AggNode : public ProcessingNode {
  using AggHashMap = AbslRowTupleHashMap<AggHashValue*>;

 public:
  AggNode() = default;
  virtual ~AggNode() = default;

 protected:
  Status AggregateGroupByNone(ExecState* exec_state, const table_store::schema::RowBatch& rb);
  Status AggregateGroupByClause(ExecState* exec_state, const table_store::schema::RowBatch& rb);

  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  AggHashMap agg_hash_map_;
  bool HasNoGroups() const { return plan_node_->groups().empty(); }
  // ReadyToEmitBatches returns true when the input stream has reached a point where output batches
  // can be emitted. In the windowed aggregate case, this happens whenever end of window (eow) is
  // reached. In the blocking aggregate case, this happens at eos only.
  bool ReadyToEmitBatches(const table_store::schema::RowBatch& rb) const;
  // When we see a new window, we need to be able to clear the aggregate state.
  Status ClearAggState(ExecState* exec_state);

  Status EvaluateSingleExpressionNoGroups(ExecState* exec_state, const UDAInfo& uda_info,
                                          plan::AggregateExpression* expr,
                                          const table_store::schema::RowBatch& rb);
  Status EvaluateAggHashValue(ExecState* exec_state, AggHashValue* val);
  StatusOr<types::DataType> GetTypeOfDep(const plan::ScalarExpression& expr) const;

  Status DeserializeAndMergeNoGroups(const RowBatch& rb);

  Status DeserializeAndMergeGrouped(const std::vector<GroupArgs>& group_args, const RowBatch& rb);

  Status DeserializeAndMergeRow(std::vector<UDAInfo>* udas, const RowBatch& rb, int64_t row_idx,
                                int64_t groups_size);

  // Store information about aggregate node from the query planner.
  std::unique_ptr<plan::AggregateOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;

  std::unique_ptr<udf::FunctionContext> function_ctx_;

  std::vector<UDAInfo> udas_for_deserialize_;

  // Variables specific to GroupByNone Agg.
  std::vector<UDAInfo> udas_no_groups_;
  // END: Variables specific to GroupByNone Agg.

  // Variables specific to GroupBy Agg.

  // As the row batches come in we insert the correct values into the hash map based
  // on the group by key. To do this we need to keep track of which input columns we need
  // to eventually run the agg funcs.
  // 1. We need to keep a mapping from plan columns to stored columns.
  std::map<int64_t, int64_t> plan_cols_to_stored_map_;
  // 2. The reverse mapping from stored_cols to plan cols. This can be a regular vector,
  // since the stored column indices are contiguous.
  std::vector<int64_t> stored_cols_to_plan_idx_;
  // 3. The data type of the stored colums, by the index they are stored at.
  std::vector<types::DataType> stored_cols_data_types_;

  ObjectPool group_args_pool_{"group_args_pool"};
  ObjectPool udas_pool_{"udas_pool"};

  std::vector<types::DataType> group_data_types_;
  std::vector<types::DataType> value_data_types_;

  // We construct row-tuples in a batch, chunked by each column.
  // This vector holds pointers to the row_tuples which are managed by the group_args_pool_.

  std::vector<GroupArgs> group_args_chunk_;
  // END: Variables specific to GroupBy Agg.

  // Creates a mapping between plan cols and stored cols (see above comment).
  Status CreateColumnMapping();

  Status ExtractRowTupleForBatch(const table_store::schema::RowBatch& rb);
  Status HashRowBatch(ExecState* exec_state, const table_store::schema::RowBatch& rb);
  Status EvaluatePartialAggregates(ExecState* exec_state, size_t num_records);
  Status ResetGroupArgs();
  Status ConvertAggHashMapToRowBatch(ExecState* exec_state,
                                     table_store::schema::RowBatch* output_rb);

  AggHashValue* CreateAggHashValue(ExecState* exec_state);
  RowTuple* CreateGroupArgsRowTuple() {
    return group_args_pool_.Add(new RowTuple(&group_data_types_));
  }

  Status CreateUDAInfoValues(std::vector<UDAInfo>* val, ExecState* exec_state);
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
