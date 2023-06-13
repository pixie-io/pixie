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

#include "src/carnot/exec/agg_node.h"

#include <arrow/array.h>
#include <arrow/array/builder_base.h>
#include <arrow/status.h>
#include <algorithm>
#include <cstdint>

#include <magic_enum.hpp>

#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf/udf_wrapper.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using SharedArray = std::shared_ptr<arrow::Array>;
constexpr int64_t kAggCompactionThreshold = 512;

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

namespace {
template <types::DataType DT>
void ExtractIntoGroupArgs(std::vector<GroupArgs>* group_args, arrow::Array* col, int rt_col_idx) {
  auto num_rows = col->length();
  for (auto row_idx = 0; row_idx < num_rows; ++row_idx) {
    ExtractIntoRowTuple<DT>((*group_args)[row_idx].rt, col, rt_col_idx, row_idx);
  }
}

template <types::DataType DT>
void AppendToBuilder(arrow::ArrayBuilder* builder, RowTuple* rt, size_t rt_idx) {
  using ArrowBuilder = typename types::DataTypeTraits<DT>::arrow_builder_type;
  using ValueType = typename types::DataTypeTraits<DT>::value_type;
  auto status =
      static_cast<ArrowBuilder*>(builder)->Append(udf::UnWrap(rt->GetValue<ValueType>(rt_idx)));
  PX_DCHECK_OK(status);
  PX_UNUSED(status);
}

template <types::DataType DT>
void ExtractToColumnWrapper(const std::vector<GroupArgs>& group_args,
                            const table_store::schema::RowBatch& rb, size_t col_idx,
                            size_t rb_col_idx) {
  size_t num_rows = rb.num_rows();
  DCHECK(num_rows <= group_args.size());
  for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
    DCHECK(group_args[row_idx].av != nullptr);
    auto col_wrapper = group_args[row_idx].av->agg_cols[col_idx].get();
    auto arr = rb.ColumnAt(rb_col_idx).get();
    types::ExtractValueToColumnWrapper<DT>(col_wrapper, arr, row_idx);
  }
}

}  // namespace

std::string AggNode::DebugStringImpl() {
  return absl::Substitute("Exec::AggNode<$0>", plan_node_->DebugString());
}

Status AggNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::AGGREGATE_OPERATOR);
  const auto* agg_plan_node = static_cast<const plan::AggregateOperator*>(&plan_node);

  // Copy the plan node to local object.
  plan_node_ = std::make_unique<plan::AggregateOperator>(*agg_plan_node);

  // Check the input_descriptors.
  if (input_descriptors_.size() != 1) {
    return error::InvalidArgument("Aggregate operator expects a single input relation, got $0",
                                  input_descriptors_.size());
  }
  input_descriptor_ = std::make_unique<RowDescriptor>(input_descriptors_[0]);

  // Check the value expressions and make sure they are correct.
  for (const auto& value : plan_node_->values()) {
    if (value->ExpressionType() != plan::Expression::kAgg) {
      return error::InvalidArgument("Aggregate operator can only use aggregate expressions");
    }
  }

  size_t output_size = plan_node_->values().size() + plan_node_->groups().size();
  if (output_size != output_descriptor_->size()) {
    return error::InvalidArgument("Output size mismatch in aggregate");
  }

  if (HasNoGroups()) {
    return Status::OK();
  }

  /**
   * Init specific for group by agg.
   */

  // Compute the group and value data types.
  // The case of GroupByNone, there will be no groups.
  auto groups_size = plan_node_->groups().size();
  group_data_types_.reserve(groups_size);
  for (const auto& group : plan_node_->groups()) {
    DCHECK(group.idx < input_descriptor_->size());
    group_data_types_.emplace_back(input_descriptor_->type(group.idx));
  }

  auto values_size = plan_node_->values().size();
  for (size_t i = 0; i < values_size; ++i) {
    auto values_idx = i + groups_size;
    DCHECK(values_idx < output_descriptor_->size());
    value_data_types_.emplace_back(output_descriptor_->type(values_idx));
  }

  return CreateColumnMapping();
}

Status AggNode::PrepareImpl(ExecState* exec_state) {
  function_ctx_ = exec_state->CreateFunctionContext();
  return Status::OK();
}

Status AggNode::OpenImpl(ExecState* exec_state) {
  if (HasNoGroups()) {
    PX_RETURN_IF_ERROR(CreateUDAInfoValues(&udas_no_groups_, exec_state));
  }
  if (!plan_node_->partial_agg()) {
    PX_RETURN_IF_ERROR(CreateUDAInfoValues(&udas_for_deserialize_, exec_state));
  }
  return Status::OK();
}

Status AggNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t) {
  if (HasNoGroups()) {
    return AggregateGroupByNone(exec_state, rb);
  }
  return AggregateGroupByClause(exec_state, rb);
}

Status AggNode::CloseImpl(ExecState*) {
  udas_no_groups_.clear();
  group_args_chunk_.clear();
  group_args_pool_.Clear();
  udas_pool_.Clear();

  return Status::OK();
}

bool AggNode::ReadyToEmitBatches(const RowBatch& rb) const {
  return rb.eos() || (rb.eow() && plan_node_->windowed());
}

Status AggNode::ClearAggState(ExecState* exec_state) {
  if (HasNoGroups()) {
    udas_no_groups_.clear();
    PX_RETURN_IF_ERROR(CreateUDAInfoValues(&udas_no_groups_, exec_state));
  }
  agg_hash_map_.clear();
  return Status::OK();
}

Status AggNode::AggregateGroupByNone(ExecState* exec_state, const RowBatch& rb) {
  auto values = plan_node_->values();
  if (plan_node_->partial_agg()) {
    for (size_t i = 0; i < values.size(); ++i) {
      PX_RETURN_IF_ERROR(
          EvaluateSingleExpressionNoGroups(exec_state, udas_no_groups_[i], values[i].get(), rb));
    }
  } else {
    PX_RETURN_IF_ERROR(DeserializeAndMergeNoGroups(rb));
  }

  if (ReadyToEmitBatches(rb)) {
    RowBatch output_rb(*output_descriptor_, 1);
    for (size_t i = 0; i < values.size(); ++i) {
      const auto& uda_info = udas_no_groups_[i];
      std::unique_ptr<arrow::ArrayBuilder> builder;
      if (plan_node_->finalize_results()) {
        builder = types::MakeArrowBuilder(uda_info.def->finalize_return_type(),
                                          exec_state->exec_mem_pool());
        PX_RETURN_IF_ERROR(
            uda_info.def->FinalizeArrow(uda_info.uda.get(), function_ctx_.get(), builder.get()));
      } else {
        builder = types::MakeArrowBuilder(types::STRING, exec_state->exec_mem_pool());
        PX_RETURN_IF_ERROR(
            uda_info.def->SerializeArrow(uda_info.uda.get(), function_ctx_.get(), builder.get()));
      }
      SharedArray out_col;
      PX_RETURN_IF_ERROR(builder->Finish(&out_col));
      PX_RETURN_IF_ERROR(output_rb.AddColumn(out_col));
    }
    output_rb.set_eow(rb.eow());
    output_rb.set_eos(rb.eos());
    PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, output_rb));
    PX_RETURN_IF_ERROR(ClearAggState(exec_state));
  }
  return Status::OK();
}

Status AggNode::ExtractRowTupleForBatch(const RowBatch& rb) {
  // Grow the group_args_chunk_ to be the size of the RowBatch.
  size_t num_rows = rb.num_rows();
  if (group_args_chunk_.size() < num_rows) {
    int prev_size = group_args_chunk_.size();
    group_args_chunk_.reserve(num_rows);
    for (size_t idx = prev_size; idx < num_rows; ++idx) {
      group_args_chunk_.emplace_back(CreateGroupArgsRowTuple());
    }
  }

  // Scan through all the group args in column order and extract the entire column.
  for (size_t idx = 0; idx < plan_node_->groups().size(); idx++) {
    auto grp = plan_node_->groups()[idx];
    DCHECK(grp.idx < input_descriptor_->size());
    DCHECK(idx < group_data_types_.size());
    auto dt = group_data_types_[idx];
    auto col = rb.ColumnAt(grp.idx).get();

#define TYPE_CASE(_dt_) ExtractIntoGroupArgs<_dt_>(&group_args_chunk_, col, idx);
    PX_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
  }
  return Status::OK();
}

Status AggNode::HashRowBatch(ExecState* exec_state, const RowBatch& rb) {
  PX_UNUSED(exec_state);
  // Loop through all the row and basically store the values into column chunk based on which
  // group they belong to.
  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    auto& ga = group_args_chunk_[row_idx];
    AggHashValue* val = nullptr;
    // Check to see if in hash
    // TODO(zasgar): Change this to upsert.
    auto it = agg_hash_map_.find(ga.rt);
    // If not in hash then insert
    if (it == agg_hash_map_.end()) {
      // Create a val array.
      val = CreateAggHashValue(exec_state);
      agg_hash_map_[ga.rt] = val;
      // We have inserted this, so the stored RowTuple is now in the table.
      ga.rt = nullptr;
    } else {
      val = it->second;
    }
    ga.av = val;
  }

  // Now extract the values in the agg hash value.
  for (size_t i = 0; i < stored_cols_data_types_.size(); ++i) {
    const auto& rb_col_idx = stored_cols_to_plan_idx_[i];
    const auto& dt = input_descriptor_->type(rb_col_idx);

    // Even if we're not doing a partial agg we want to extract the group values.
    if (plan_node_->partial_agg() || i < plan_node_->groups().size()) {
#define TYPE_CASE(_dt_) ExtractToColumnWrapper<_dt_>(group_args_chunk_, rb, i, rb_col_idx);

      PX_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
    }
  }
  if (!plan_node_->partial_agg()) {
    // If we're not performing a partial_agg, then we're receiving serialized partial aggs, so we
    // deserialize and merge them here.
    PX_RETURN_IF_ERROR(DeserializeAndMergeGrouped(group_args_chunk_, rb));
  }

  return Status::OK();
}

Status AggNode::EvaluatePartialAggregates(ExecState* exec_state, size_t num_records) {
  PX_UNUSED(exec_state);
  // TODO(zasgar): This only needs to run for unique groups. We should find
  // a way to optimize this.
  for (size_t i = 0; i < num_records; ++i) {
    DCHECK(i < group_args_chunk_.size());
    auto& ga = group_args_chunk_[i];
    DCHECK(ga.av != nullptr);
    if (ga.av->agg_cols[0]->Size() > kAggCompactionThreshold) {
      PX_RETURN_IF_ERROR(EvaluateAggHashValue(exec_state, ga.av));
    }
  }
  return Status::OK();
}

Status AggNode::ResetGroupArgs() {
  // Reset the group args. If the row tuple is null it has been consumed, so
  // we can replace it with a new RowTuple. We also reset the
  // agg hash value to nullptr.
  for (size_t i = 0; i < group_args_chunk_.size(); ++i) {
    group_args_chunk_[i].av = nullptr;
    if (group_args_chunk_[i].rt == nullptr) {
      group_args_chunk_[i].rt = CreateGroupArgsRowTuple();
    } else {
      group_args_chunk_[i].rt->Reset();
    }
  }
  return Status::OK();
}

Status AggNode::ConvertAggHashMapToRowBatch(ExecState* exec_state, RowBatch* output_rb) {
  PX_UNUSED(exec_state);
  DCHECK(output_rb != nullptr);
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> group_builders;
  for (const auto& group_dt : group_data_types_) {
    group_builders.push_back(types::MakeArrowBuilder(group_dt, exec_state->exec_mem_pool()));
  }
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> value_builders;
  for (const auto& value_data_type : value_data_types_) {
    value_builders.push_back(types::MakeArrowBuilder(value_data_type, exec_state->exec_mem_pool()));
  }

  // Agg into agg values and emit!
  for (const auto& kv : agg_hash_map_) {
    auto* groups_rt = kv.first;
    auto* val = kv.second;

    for (size_t i = 0; i < group_data_types_.size(); ++i) {
      DCHECK(i < group_builders.size());

#define TYPE_CASE(_dt_) AppendToBuilder<_dt_>(group_builders[i].get(), groups_rt, i);
      PX_SWITCH_FOREACH_DATATYPE(group_data_types_[i], TYPE_CASE);
#undef TYPE_CASE
    }
    if (plan_node_->partial_agg()) {
      PX_RETURN_IF_ERROR(EvaluateAggHashValue(exec_state, val));
    }

    if (plan_node_->finalize_results()) {
      // Actually Finalize the UDA based on the column wrapper chunks.
      for (size_t i = 0; i < val->udas.size(); ++i) {
        const auto& uda_info = val->udas[i];
        PX_RETURN_IF_ERROR(uda_info.def->FinalizeArrow(uda_info.uda.get(), function_ctx_.get(),
                                                       value_builders[i].get()));
      }
    } else {
      for (size_t i = 0; i < val->udas.size(); ++i) {
        const auto& uda_info = val->udas[i];
        PX_RETURN_IF_ERROR(uda_info.def->SerializeArrow(uda_info.uda.get(), function_ctx_.get(),
                                                        value_builders[i].get()));
      }
    }
  }

  for (const auto& group_builder : group_builders) {
    std::shared_ptr<arrow::Array> arr;
    PX_RETURN_IF_ERROR(group_builder->Finish(&arr));
    PX_RETURN_IF_ERROR(output_rb->AddColumn(arr));
  }

  for (const auto& value_builder : value_builders) {
    std::shared_ptr<arrow::Array> arr;
    PX_RETURN_IF_ERROR(value_builder->Finish(&arr));
    PX_RETURN_IF_ERROR(output_rb->AddColumn(arr));
  }

  return Status::OK();
}

Status AggNode::AggregateGroupByClause(ExecState* exec_state, const RowBatch& rb) {
  // Extracts the row tuples (column wise).
  // TODO(zasgar): PL-455 - Chunk this so we don't create a crazy number of row tuples if the batch
  // is large. The process is as follows:
  // 1. Extract each column into the appropriate part of the row tuple.
  // 2. Hash row batch and update agg values.
  // 3. If the agg values are large then run aggregate and compact.
  // 4. Reset state to prepare for next row batch.
  // 5. If it's the last batch then emit the values.
  PX_RETURN_IF_ERROR(ExtractRowTupleForBatch(rb));
  PX_RETURN_IF_ERROR(HashRowBatch(exec_state, rb));
  if (plan_node_->partial_agg() && plan_node_->values().size() > 0) {
    PX_RETURN_IF_ERROR(EvaluatePartialAggregates(exec_state, rb.num_rows()));
  }
  PX_RETURN_IF_ERROR(ResetGroupArgs());
  if (ReadyToEmitBatches(rb)) {
    RowBatch output_rb(*output_descriptor_, agg_hash_map_.size());
    PX_RETURN_IF_ERROR(ConvertAggHashMapToRowBatch(exec_state, &output_rb));
    output_rb.set_eow(rb.eow());
    output_rb.set_eos(rb.eos());
    PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, output_rb));
    PX_RETURN_IF_ERROR(ClearAggState(exec_state));
  }
  return Status::OK();
}

StatusOr<types::DataType> AggNode::GetTypeOfDep(const plan::ScalarExpression& expr) const {
  // Agg exprs can only be of type col, or  const.
  switch (expr.ExpressionType()) {
    case plan::Expression::kColumn: {
      auto idx = static_cast<const plan::Column*>(&expr)->Index();
      return input_descriptor_->type(idx);
    }
    case plan::Expression::kConstant:
      return static_cast<const plan::ScalarValue*>(&expr)->DataType();
    default:
      return error::InvalidArgument("Invalid expression type in agg: $0",
                                    magic_enum::enum_name(expr.ExpressionType()));
  }
}

Status AggNode::EvaluateSingleExpressionNoGroups(ExecState* exec_state, const UDAInfo& uda_info,
                                                 plan::AggregateExpression* expr,
                                                 const RowBatch& input_rb) {
  plan::ExpressionWalker<StatusOr<SharedArray>> walker;
  walker.OnScalarValue(
      [&](const plan::ScalarValue& val,
          const std::vector<StatusOr<SharedArray>>& children) -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return EvalScalarToArrow(exec_state, val, input_rb.num_rows());
      });

  walker.OnColumn(
      [&](const plan::Column& col,
          const std::vector<StatusOr<SharedArray>>& children) -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return input_rb.ColumnAt(col.Index());
      });

  walker.OnAggregateExpression(
      [&](const plan::AggregateExpression& agg,
          const std::vector<StatusOr<SharedArray>>& children) -> StatusOr<SharedArray> {
        DCHECK(agg.name() == uda_info.def->name());
        DCHECK(children.size() == uda_info.def->update_arguments().size());
        // collect the arguments.
        std::vector<const arrow::Array*> raw_children;
        raw_children.reserve(children.size());
        for (const auto& child : children) {
          if (!child.ok()) {
            return child;
          }
          raw_children.push_back(child.ValueOrDie().get());
        }
        PX_RETURN_IF_ERROR(uda_info.def->ExecBatchUpdateArrow(uda_info.uda.get(), nullptr /* ctx */,
                                                              raw_children));
        // Blocking aggregates don't produce results until all data is seen.
        return {};
      });

  PX_RETURN_IF_ERROR(walker.Walk(*expr));
  return Status::OK();
}

Status AggNode::EvaluateAggHashValue(ExecState* exec_state, AggHashValue* val) {
  size_t values_size = plan_node_->values().size();
  for (size_t i = 0; i < values_size; ++i) {
    const auto& uda_info = val->udas[i];
    const auto& expr = *plan_node_->values()[i];
    size_t num_records = val->agg_cols[0]->Size();
    plan::ExpressionWalker<StatusOr<types::SharedColumnWrapper>> walker;
    walker.OnScalarValue([&](const plan::ScalarValue& scalar_val,
                             const std::vector<StatusOr<types::SharedColumnWrapper>>& children)
                             -> types::SharedColumnWrapper {
      DCHECK_EQ(children.size(), 0ULL);
      return EvalScalarToColumnWrapper(exec_state, scalar_val, num_records);
    });

    walker.OnColumn([&](const plan::Column& col,
                        const std::vector<StatusOr<types::SharedColumnWrapper>>& children)
                        -> types::SharedColumnWrapper {
      DCHECK_EQ(children.size(), 0ULL);
      return val->agg_cols[plan_cols_to_stored_map_[col.Index()]];
    });

    walker.OnAggregateExpression(
        [&](const plan::AggregateExpression& agg,
            const std::vector<StatusOr<types::SharedColumnWrapper>>& children)
            -> StatusOr<types::SharedColumnWrapper> {
          DCHECK(agg.name() == uda_info.def->name());
          DCHECK(children.size() == uda_info.def->update_arguments().size());
          // collect the arguments.
          std::vector<const types::ColumnWrapper*> raw_children;
          raw_children.reserve(children.size());
          for (auto& child : children) {
            PX_RETURN_IF_ERROR(child);
            raw_children.push_back(child.ValueOrDie().get());
          }
          PX_RETURN_IF_ERROR(
              uda_info.def->ExecBatchUpdate(uda_info.uda.get(), nullptr /* ctx */, raw_children));
          // Blocking aggregates don't produce results until all data is seen.
          return {};
        });
    PX_RETURN_IF_ERROR(walker.Walk(expr));
  }

  for (auto& col : val->agg_cols) {
    // Clear the values, so we don't aggregate them twice.
    col->Clear();
  }
  return Status::OK();
}

Status AggNode::CreateColumnMapping() {
  if (!plan_node_->partial_agg()) {
    // If we're deserializing partial aggregates, then we have 1 column for each group and 1 column
    // for each value.
    auto groups_size = static_cast<int64_t>(plan_node_->groups().size());
    auto values_size = static_cast<int64_t>(plan_node_->values().size());
    for (int64_t i = 0; i < groups_size + values_size; i++) {
      plan_cols_to_stored_map_[i] = i;
      stored_cols_to_plan_idx_.emplace_back(i);
      stored_cols_data_types_.emplace_back(input_descriptor_->type(i));
    }
    return Status::OK();
  }
  for (const auto& expr : plan_node_->values()) {
    plan::ExpressionWalker<int> walker;

    walker.OnScalarValue(
        [&](const plan::ScalarValue&, const std::vector<int>&) -> int { return 0; });

    walker.OnColumn([&](const plan::Column& col, const std::vector<int>&) -> int {
      auto plan_col_idx = col.Index();
      if (plan_cols_to_stored_map_.find(plan_col_idx) == plan_cols_to_stored_map_.end()) {
        // We aren't currently capturing this col, so add it to the list of cols to store.
        plan_cols_to_stored_map_[plan_col_idx] = stored_cols_to_plan_idx_.size();
        stored_cols_to_plan_idx_.emplace_back(plan_col_idx);
        stored_cols_data_types_.emplace_back(input_descriptor_->type(plan_col_idx));
      }
      return 0;
    });

    walker.OnAggregateExpression(
        [&](const plan::AggregateExpression&, const std::vector<int>&) -> int { return 0; });

    PX_RETURN_IF_ERROR(walker.Walk(*expr));
  }
  return Status::OK();
}

AggHashValue* AggNode::CreateAggHashValue(ExecState* exec_state) {
  auto* val = udas_pool_.Add(new AggHashValue);
  PX_CHECK_OK(CreateUDAInfoValues(&(val->udas), exec_state));
  for (const auto& dt : stored_cols_data_types_) {
    val->agg_cols.emplace_back(types::ColumnWrapper::Make(dt, 0));
  }
  return val;
}

Status AggNode::CreateUDAInfoValues(std::vector<UDAInfo>* val, ExecState* exec_state) {
  CHECK(val != nullptr);
  CHECK_EQ(val->size(), 0ULL);

  for (const auto& value : plan_node_->values()) {
    auto def = exec_state->GetUDADefinition(value->uda_id());
    auto uda = def->Make();

    // We only init the UDAs if we're doing the partial agg ourself. If another node did the partial
    // agg, then this node will deserialize and merge into these UDAs, so there's no need for init.
    if (plan_node_->partial_agg()) {
      std::vector<std::shared_ptr<types::BaseValueType>> init_args;
      for (const auto& arg : value->init_arguments()) {
        init_args.push_back(arg.ToBaseValueType());
      }
      // We currently don't use FunctionContext in UDAs so continuing that tradition here, but at
      // some point we probably want to change this.
      PX_RETURN_IF_ERROR(def->ExecInit(uda.get(), nullptr, init_args));
    }
    val->emplace_back(std::move(uda), def);
  }
  return Status::OK();
}

Status AggNode::DeserializeAndMergeNoGroups(const RowBatch& rb) {
  for (int64_t row_idx = 0; row_idx < rb.num_rows(); row_idx++) {
    PX_RETURN_IF_ERROR(DeserializeAndMergeRow(&udas_no_groups_, rb, row_idx, 0));
  }
  return Status::OK();
}

Status AggNode::DeserializeAndMergeGrouped(const std::vector<GroupArgs>& group_args,
                                           const RowBatch& rb) {
  auto groups_size = static_cast<int64_t>(plan_node_->groups().size());
  for (int64_t row_idx = 0; row_idx < rb.num_rows(); row_idx++) {
    DCHECK(group_args[row_idx].av != nullptr);
    PX_RETURN_IF_ERROR(
        DeserializeAndMergeRow(&group_args[row_idx].av->udas, rb, row_idx, groups_size));
  }

  return Status::OK();
}

Status AggNode::DeserializeAndMergeRow(std::vector<UDAInfo>* udas, const RowBatch& rb,
                                       int64_t row_idx, int64_t groups_size) {
  for (size_t uda_idx = 0; uda_idx < udas->size(); ++uda_idx) {
    auto& deserial_uda_info = udas_for_deserialize_[uda_idx];
    auto& merge_uda_info = (*udas)[uda_idx];
    int64_t col_idx = groups_size + static_cast<int64_t>(uda_idx);
    DCHECK_EQ(types::STRING, rb.desc().type(col_idx));
    auto serialized =
        types::GetValueFromArrowArray<types::STRING>(rb.ColumnAt(col_idx).get(), row_idx);
    PX_RETURN_IF_ERROR(deserial_uda_info.def->Deserialize(deserial_uda_info.uda.get(),
                                                          function_ctx_.get(), serialized));
    PX_RETURN_IF_ERROR(merge_uda_info.def->Merge(merge_uda_info.uda.get(),
                                                 deserial_uda_info.uda.get(), function_ctx_.get()));
  }
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
