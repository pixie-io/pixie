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

#include "src/carnot/plan/operators.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>
#include <google/protobuf/text_format.h>
#include <magic_enum.hpp>

#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf_definition.h"
#include "src/carnot/udf/udtf.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace plan {

using px::Status;

template <typename TOp, typename TProto>
std::unique_ptr<Operator> CreateOperator(int64_t id, const TProto& pb) {
  auto op = std::make_unique<TOp>(id);
  auto s = op->Init(pb);
  // On init failure, return null;
  if (!s.ok()) {
    LOG(ERROR) << "Failed to initialize operator with err: " << s.msg();
    return nullptr;
  }
  return op;
}

std::unique_ptr<Operator> Operator::FromProto(const planpb::Operator& pb, int64_t id) {
  switch (pb.op_type()) {
    case planpb::MEMORY_SOURCE_OPERATOR:
      return CreateOperator<MemorySourceOperator>(id, pb.mem_source_op());
    case planpb::MAP_OPERATOR:
      return CreateOperator<MapOperator>(id, pb.map_op());
    case planpb::AGGREGATE_OPERATOR:
      return CreateOperator<AggregateOperator>(id, pb.agg_op());
    case planpb::MEMORY_SINK_OPERATOR:
      return CreateOperator<MemorySinkOperator>(id, pb.mem_sink_op());
    case planpb::GRPC_SOURCE_OPERATOR:
      return CreateOperator<GRPCSourceOperator>(id, pb.grpc_source_op());
    case planpb::GRPC_SINK_OPERATOR:
      return CreateOperator<GRPCSinkOperator>(id, pb.grpc_sink_op());
    case planpb::FILTER_OPERATOR:
      return CreateOperator<FilterOperator>(id, pb.filter_op());
    case planpb::LIMIT_OPERATOR:
      return CreateOperator<LimitOperator>(id, pb.limit_op());
    case planpb::UNION_OPERATOR:
      return CreateOperator<UnionOperator>(id, pb.union_op());
    case planpb::JOIN_OPERATOR:
      return CreateOperator<JoinOperator>(id, pb.join_op());
    case planpb::UDTF_SOURCE_OPERATOR:
      return CreateOperator<UDTFSourceOperator>(id, pb.udtf_source_op());
    case planpb::EMPTY_SOURCE_OPERATOR:
      return CreateOperator<EmptySourceOperator>(id, pb.empty_source_op());
    case planpb::OTEL_EXPORT_SINK_OPERATOR:
      return CreateOperator<OTelExportSinkOperator>(id, pb.otel_sink_op());
    default:
      LOG(FATAL) << absl::Substitute("Unknown operator type: $0",
                                     magic_enum::enum_name(pb.op_type()));
  }
}

/**
 * Memory Source Operator Implementation.
 */

std::string MemorySourceOperator::DebugString() const {
  return absl::Substitute("Op:MemorySource($0, [$1], start=$2, end=$3, streaming=$4)", TableName(),
                          absl::StrJoin(Columns(), ","), start_time(), stop_time(), streaming());
}

Status MemorySourceOperator::Init(const planpb::MemorySourceOperator& pb) {
  pb_ = pb;
  column_idxs_.reserve(static_cast<size_t>(pb_.column_idxs_size()));
  for (int i = 0; i < pb_.column_idxs_size(); ++i) {
    column_idxs_.emplace_back(pb_.column_idxs(i));
  }
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> MemorySourceOperator::OutputRelation(
    const table_store::schema::Schema&, const PlanState&,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";
  if (!input_ids.empty()) {
    // TODO(zasgar): We should figure out if we need to treat the "source table" as
    // an input relation.
    return error::InvalidArgument("Source operator cannot have any inputs");
  }
  table_store::schema::Relation r;
  for (int i = 0; i < pb_.column_idxs_size(); ++i) {
    r.AddColumn(pb_.column_types(i), pb_.column_names(i));
  }
  return r;
}

/**
 * Map Operator Implementation.
 */

std::string MapOperator::DebugString() const {
  std::string debug_string;
  debug_string += "(";
  for (size_t i = 0; i < expressions_.size(); ++i) {
    if (i != 0u) {
      debug_string += ",";
    }
    debug_string += absl::Substitute("$0:$1", column_names_[i], expressions_[i]->DebugString());
  }
  debug_string += ")";
  return "Op:Map" + debug_string;
}

Status MapOperator::Init(const planpb::MapOperator& pb) {
  pb_ = pb;
  // Some sanity tests.
  if (pb_.column_names_size() != pb_.expressions_size()) {
    return error::InvalidArgument("Column names and expressions need the same size");
  }

  column_names_.reserve(static_cast<size_t>(pb_.column_names_size()));
  expressions_.reserve(static_cast<size_t>(pb_.expressions_size()));
  for (int i = 0; i < pb_.expressions_size(); ++i) {
    column_names_.emplace_back(pb_.column_names(i));
    auto s = ScalarExpression::FromProto(pb_.expressions(i));
    PX_RETURN_IF_ERROR(s);
    expressions_.emplace_back(s.ConsumeValueOrDie());
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> MapOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState& state,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";
  if (input_ids.size() != 1) {
    return error::InvalidArgument("Map operator must have exactly one input");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing relation ($0) for input of Map", input_ids[0]);
  }
  table_store::schema::Relation r;
  for (size_t idx = 0; idx < expressions_.size(); ++idx) {
    auto s = expressions_[idx]->OutputDataType(state, schema);
    PX_RETURN_IF_ERROR(s);
    r.AddColumn(s.ConsumeValueOrDie(), column_names_[idx]);
  }
  return r;
}

/**
 * Aggregate Operator Implementation.
 */

std::string AggregateOperator::DebugString() const {
  const auto& v = values();
  std::vector<std::string> value_names(v.size());
  std::transform(begin(v), end(v), begin(value_names), [](auto val) { return val->name(); });

  const auto& g = groups();
  std::vector<std::string> group_names(g.size());
  std::transform(begin(g), end(g), begin(group_names), [](auto val) { return val.name; });
  std::string out;
  ::google::protobuf::TextFormat::PrintToString(pb_, &out);
  return absl::Substitute(
      "Op:Aggregate(values=($0), groups=($1), partial=($2), finalize=($3)):\n$4",
      absl::StrJoin(value_names, ", "), absl::StrJoin(group_names, ", "), partial_agg(),
      finalize_results(), out);
}

Status AggregateOperator::Init(const planpb::AggregateOperator& pb) {
  pb_ = pb;
  if (pb_.groups_size() != pb_.group_names_size()) {
    return error::InvalidArgument("group names/exp size mismatch");
  }
  if (pb_.values_size() != pb_.value_names_size()) {
    return error::InvalidArgument("values names/exp size mismatch");
  }
  values_.reserve(static_cast<size_t>(pb_.values_size()));
  for (int i = 0; i < pb_.values_size(); ++i) {
    auto ae = std::make_unique<AggregateExpression>();
    auto s = ae->Init(pb_.values(i));
    PX_RETURN_IF_ERROR(s);
    values_.emplace_back(std::unique_ptr<AggregateExpression>(std::move(ae)));
  }
  groups_.reserve(pb_.groups_size());
  for (int idx = 0; idx < pb_.groups_size(); ++idx) {
    groups_.emplace_back(GroupInfo{pb_.group_names(idx), pb_.groups(idx).index()});
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> AggregateOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState& state,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";
  if (input_ids.size() != 1) {
    return error::InvalidArgument("BlockingAgg operator must have exactly one input");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing relation ($0) for input of BlockingAggregateOperator",
                           input_ids[0]);
  }

  PX_ASSIGN_OR_RETURN(const auto& input_relation, schema.GetRelation(input_ids[0]));
  table_store::schema::Relation output_relation;

  for (int idx = 0; idx < pb_.groups_size(); ++idx) {
    int64_t node_id = pb_.groups(idx).node();
    int64_t col_idx = pb_.groups(idx).index();
    if (node_id != input_ids[0]) {
      return error::InvalidArgument("Column $0 does not belong to the correct input node $1",
                                    col_idx, node_id);
    }
    if (col_idx > static_cast<int64_t>(input_relation.NumColumns())) {
      return error::InvalidArgument("Column index $0 is out of bounds for node $1", col_idx,
                                    node_id);
    }
    output_relation.AddColumn(input_relation.GetColumnType(col_idx), pb_.group_names(idx));
  }

  for (const auto& [i, value] : Enumerate(values_)) {
    if (pb_.finalize_results()) {
      PX_ASSIGN_OR_RETURN(auto dt, value->OutputDataType(state, schema));
      output_relation.AddColumn(dt, pb_.value_names(i));
    } else {
      output_relation.AddColumn(types::STRING, pb_.value_names(i));
    }
  }
  return output_relation;
}

/**
 * Memory Sink Operator Implementation.
 */

std::string MemorySinkOperator::DebugString() const { return "Op:MemorySink"; }

Status MemorySinkOperator::Init(const planpb::MemorySinkOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> MemorySinkOperator::OutputRelation(
    const table_store::schema::Schema&, const PlanState&, const std::vector<int64_t>&) const {
  DCHECK(is_initialized_) << "Not initialized";
  // There are no outputs.
  return table_store::schema::Relation();
}

/**
 * GRPC Source Operator Implementation.
 */

std::string GRPCSourceOperator::DebugString() const {
  return absl::Substitute("Op:GRPCSource($0)", absl::StrJoin(pb_.column_names(), ","));
}

Status GRPCSourceOperator::Init(const planpb::GRPCSourceOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> GRPCSourceOperator::OutputRelation(
    const table_store::schema::Schema&, const PlanState&,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";
  if (!input_ids.empty()) {
    // See MemorySourceOperator TODO. We might want to make source inputs an input relation.
    return error::InvalidArgument("Source operator cannot have any inputs");
  }
  table_store::schema::Relation r;
  for (int i = 0; i < pb_.column_types_size(); ++i) {
    r.AddColumn(pb_.column_types(i), pb_.column_names(i));
  }
  return r;
}

/**
 * GRPC Sink Operator Implementation.
 */

std::string GRPCSinkOperator::DebugString() const {
  std::string destination;
  if (has_table_name()) {
    destination = absl::Substitute("table_name=$0", table_name());
  } else if (has_grpc_source_id()) {
    destination = absl::Substitute("source_id=$0", grpc_source_id());
  }
  return absl::Substitute("Op:GRPCSink($0, $1)", address(), destination);
}

Status GRPCSinkOperator::Init(const planpb::GRPCSinkOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> GRPCSinkOperator::OutputRelation(
    const table_store::schema::Schema&, const PlanState&, const std::vector<int64_t>&) const {
  DCHECK(is_initialized_) << "Not initialized";
  // There are no outputs.
  return table_store::schema::Relation();
}

/**
 * Filter Operator Implementation.
 */
std::string FilterOperator::DebugString() const {
  std::string debug_string = absl::Substitute("($0, selected=[$1])", expression_->DebugString(),
                                              absl::StrJoin(selected_cols_, ","));
  return "Op:Filter" + debug_string;
}

Status FilterOperator::Init(const planpb::FilterOperator& pb) {
  pb_ = pb;
  PX_ASSIGN_OR_RETURN(expression_, ScalarExpression::FromProto(pb_.expression()));

  selected_cols_.reserve(pb_.columns_size());
  for (auto i = 0; i < pb_.columns_size(); ++i) {
    selected_cols_.push_back(pb_.columns(i).index());
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> FilterOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState& /*state*/,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";

  if (input_ids.size() != 1) {
    return error::InvalidArgument("Filter operator must have exactly one input");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing relation ($0) for input of FilterOperator", input_ids[0]);
  }

  for (auto i = 0; i < pb_.columns_size(); ++i) {
    int64_t node = pb_.columns(i).node();
    if (node != input_ids[0]) {
      return error::InvalidArgument(
          "Column $0 does not belong to the expected input node $1, got $2", i, input_ids[0], node);
    }
  }

  PX_ASSIGN_OR_RETURN(auto input_relation, schema.GetRelation(input_ids[0]));
  table_store::schema::Relation output_relation;
  for (auto selected_col_idx : selected_cols_) {
    CHECK_LT(selected_col_idx, static_cast<int64_t>(input_relation.NumColumns()))
        << absl::Substitute("Column index $0 is out of bounds, number of columns is $1",
                            selected_col_idx, input_relation.NumColumns());

    output_relation.AddColumn(input_relation.GetColumnType(selected_col_idx),
                              input_relation.GetColumnName(selected_col_idx),
                              input_relation.GetColumnDesc(selected_col_idx));
  }

  return output_relation;
}

/**
 * Limit Operator Implementation.
 */
std::string LimitOperator::DebugString() const {
  std::string debug_string =
      absl::Substitute("($0, cols: [$1])", record_limit_, absl::StrJoin(selected_cols_, ","));
  return "Op:Limit" + debug_string;
}

Status LimitOperator::Init(const planpb::LimitOperator& pb) {
  pb_ = pb;
  record_limit_ = pb_.limit();

  selected_cols_.reserve(pb_.columns_size());
  for (auto i = 0; i < pb_.columns_size(); ++i) {
    selected_cols_.push_back(pb_.columns(i).index());
  }

  abortable_srcs_.reserve(pb_.abortable_srcs_size());
  for (auto i = 0; i < pb_.abortable_srcs_size(); ++i) {
    abortable_srcs_.push_back(pb_.abortable_srcs(i));
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> LimitOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState& /*state*/,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";

  if (input_ids.size() != 1) {
    return error::InvalidArgument("Filter operator must have exactly one input");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing relation ($0) for input of FilterOperator", input_ids[0]);
  }

  PX_ASSIGN_OR_RETURN(const table_store::schema::Relation& input_relation,
                      schema.GetRelation(input_ids[0]));
  table_store::schema::Relation output_relation;
  for (auto selected_col_idx : selected_cols_) {
    CHECK_LT(selected_col_idx, static_cast<int64_t>(input_relation.NumColumns()))
        << absl::Substitute("Column index $0 is out of bounds, number of columns is $1",
                            selected_col_idx, input_relation.NumColumns());

    output_relation.AddColumn(input_relation.GetColumnType(selected_col_idx),
                              input_relation.GetColumnName(selected_col_idx),
                              input_relation.GetColumnDesc(selected_col_idx));
  }

  // Output relation is the same as the input relation.
  return output_relation;
}

/**
 * Zip Operator Implementation.
 */
std::string UnionOperator::DebugString() const {
  return absl::Substitute("Op:Union(columns=($0)", absl::StrJoin(column_names_, ","));
}

Status UnionOperator::Init(const planpb::UnionOperator& pb) {
  pb_ = pb;

  column_names_.reserve(static_cast<size_t>(pb_.column_names_size()));
  for (int i = 0; i < pb_.column_names_size(); ++i) {
    column_names_.emplace_back(pb_.column_names(i));
  }

  column_mappings_.reserve(static_cast<size_t>(pb_.column_mappings_size()));

  for (int i = 0; i < pb_.column_mappings_size(); ++i) {
    if (pb_.column_mappings(i).column_indexes_size() != pb_.column_names_size()) {
      return error::InvalidArgument(
          "Inconsistent number of columns in UnionOperator, expected $0 but received $1 for input "
          "$2.",
          pb_.column_names_size(), pb_.column_mappings(i).column_indexes_size(), i);
    }
    std::vector<int64_t> mapping;
    for (int output_index : pb_.column_mappings(i).column_indexes()) {
      mapping.emplace_back(output_index);
    }
    column_mappings_.emplace_back(mapping);
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> UnionOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState& /*state*/,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";

  if (input_ids.size() != column_mappings_.size()) {
    return error::InvalidArgument("UnionOperator expected $0 input relations but received $1",
                                  column_mappings_.size(), input_ids.size());
  }

  // Keep track of the expected types for each output column.
  table_store::schema::Relation r;

  // Parse each input and make sure the contents match what we expected to see.
  for (size_t i = 0; i < input_ids.size(); ++i) {
    if (!schema.HasRelation(input_ids[i])) {
      return error::NotFound("Missing relation ($0) for input of UnionOperator", input_ids[i]);
    }

    PX_ASSIGN_OR_RETURN(const auto& input_relation, schema.GetRelation(input_ids[i]));

    for (size_t output_index = 0; output_index < column_mapping(i).size(); ++output_index) {
      auto src_index = column_mapping(i).at(output_index);  // Source index of output column j
      if (!input_relation.HasColumn(src_index)) {
        return error::InvalidArgument("Missing column $0 of input $1 in UnionOperator", src_index,
                                      i);
      }
      auto col_type = input_relation.GetColumnType(src_index);  // Type of output column j

      if (i == 0) {
        r.AddColumn(col_type, column_names_[output_index]);
        continue;
      }
      if (r.GetColumnType(output_index) != col_type) {
        return error::InvalidArgument("Conflicting types for column ($0) in UnionOperator",
                                      column_names_[output_index]);
      }
    }
  }

  return r;
}

int64_t time_column_idx(const std::vector<std::string>& column_names) {
  auto it = std::find(column_names.begin(), column_names.end(), "time_");
  if (it == column_names.end()) {
    return -1;
  }
  return std::distance(column_names.begin(), it);
}

bool has_time_column(const std::vector<std::string>& column_names) {
  return time_column_idx(column_names) >= 0;
}

bool UnionOperator::order_by_time() const { return has_time_column(column_names()); }

int64_t UnionOperator::time_column_index(int64_t parent_index) const {
  auto output_idx = time_column_idx(column_names());
  DCHECK_GE(output_idx, 0);
  return column_mappings_.at(parent_index).at(output_idx);
}

/**
 * Join Operator Implementation.
 */

std::string JoinOperator::DebugString(planpb::JoinOperator::JoinType type) {
  return std::string(magic_enum::enum_name(type));
}

std::string JoinOperator::DebugString(
    const std::vector<planpb::JoinOperator::EqualityCondition>& conditions) {
  std::vector<std::string> strs(conditions.size());
  for (const auto& condition : conditions) {
    strs.push_back(absl::Substitute("parent[0][$0] == parent[1][$1]", condition.left_column_index(),
                                    condition.right_column_index()));
  }
  return absl::StrJoin(strs, " && ");
}

std::string JoinOperator::DebugString() const {
  return absl::Substitute("Op:JoinOperator(type='$0', condition=($1), output_columns=($2))",
                          DebugString(type()), DebugString(equality_conditions()),
                          absl::StrJoin(column_names_, ","));
}

Status JoinOperator::Init(const planpb::JoinOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;

  DCHECK_EQ(pb_.column_names_size(), pb_.output_columns_size());

  column_names_.reserve(static_cast<size_t>(pb_.column_names_size()));
  output_columns_.reserve(static_cast<size_t>(pb_.column_names_size()));
  for (auto i = 0; i < pb_.column_names_size(); ++i) {
    column_names_.emplace_back(pb_.column_names(i));
    output_columns_.emplace_back(pb_.output_columns(i));
  }

  equality_conditions_.reserve(static_cast<size_t>(pb_.equality_conditions_size()));
  for (auto i = 0; i < pb_.equality_conditions_size(); ++i) {
    equality_conditions_.emplace_back(pb_.equality_conditions(i));
  }

  if (order_by_time()) {
    // Only support inner joins and left joins where the time_ column comes from the left table.
    // We need a time_ value for every output row in the ordered case to preserve time ordering.
    if (type() == planpb::JoinOperator::FULL_OUTER) {
      return error::InvalidArgument("For time ordered joins, full outer join is not supported.");
    }
    if (type() == planpb::JoinOperator::LEFT_OUTER && time_column().parent_index() != 0) {
      return error::InvalidArgument(
          "For time ordered joins, left join is only supported when time_ comes from the left "
          "table.");
    }
  }

  return Status::OK();
}

StatusOr<table_store::schema::Relation> JoinOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState&,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";
  if (input_ids.size() != 2) {
    return error::InvalidArgument("Join operator must have two input tables.");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing left table relation ($0) for input of Join operator",
                           input_ids[0]);
  }
  if (!schema.HasRelation(input_ids[1])) {
    return error::NotFound("Missing right table relation ($0) for input of Join operator",
                           input_ids[1]);
  }

  PX_ASSIGN_OR_RETURN(const auto& left_relation, schema.GetRelation(input_ids[0]));
  PX_ASSIGN_OR_RETURN(const auto& right_relation, schema.GetRelation(input_ids[1]));

  table_store::schema::Relation r;
  for (int i = 0; i < pb_.column_names_size(); ++i) {
    const auto& input_column = pb_.output_columns(i);
    auto type = input_column.parent_index() == 0
                    ? left_relation.GetColumnType(input_column.column_index())
                    : right_relation.GetColumnType(input_column.column_index());
    r.AddColumn(type, pb_.column_names(i));
  }
  return r;
}

bool JoinOperator::order_by_time() const { return has_time_column(column_names()); }

planpb::JoinOperator::ParentColumn JoinOperator::time_column() const {
  DCHECK(order_by_time());
  auto pos = std::distance(column_names().begin(),
                           std::find(column_names().begin(), column_names().end(), "time_"));
  return output_columns()[pos];
}

Status UDTFSourceOperator::Init(const planpb::UDTFSourceOperator& pb) {
  pb_ = pb;

  for (const auto& sv : pb_.arg_values()) {
    ScalarValue s;
    PX_RETURN_IF_ERROR(s.Init(sv));
    init_arguments_.emplace_back(s);
  }
  return Status::OK();
}

StatusOr<table_store::schema::Relation> UDTFSourceOperator::OutputRelation(
    const table_store::schema::Schema& /*schema*/, const PlanState& state,
    const std::vector<int64_t>& /*input_ids*/) const {
  PX_ASSIGN_OR_RETURN(auto def, state.func_registry()->GetUDTFDefinition(pb_.name()));
  auto cols = def->output_relation();

  table_store::schema::Relation output_rel;
  for (const auto& c : cols) {
    output_rel.AddColumn(c.type(), std::string(c.name()));
  }
  return output_rel;
}

std::string UDTFSourceOperator::DebugString() const {
  return absl::Substitute("UDTFSource<$0>", pb_.name());
}

const std::vector<ScalarValue>& UDTFSourceOperator::init_arguments() const {
  return init_arguments_;
}

/**
 *  EmptySourceOperator definition.
 */

std::string EmptySourceOperator::DebugString() const { return "Op:EmptySource"; }

Status EmptySourceOperator::Init(const planpb::EmptySourceOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> EmptySourceOperator::OutputRelation(
    const table_store::schema::Schema&, const PlanState&,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";
  if (!input_ids.empty()) {
    // TODO(zasgar): We should figure out if we need to treat the "source table" as
    // an input relation.
    return error::InvalidArgument("Source operator cannot have any inputs");
  }
  table_store::schema::Relation r;
  for (int i = 0; i < pb_.column_types_size(); ++i) {
    r.AddColumn(pb_.column_types(i), pb_.column_names(i));
  }
  return r;
}

/**
 * OTel Export Sink Operator Implementation.
 */

std::string OTelExportSinkOperator::DebugString() const { return "Op:OTelExportSink()"; }

Status OTelExportSinkOperator::Init(const planpb::OTelExportSinkOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  for (const auto& [key, value] : pb_.endpoint_config().headers()) {
    headers_.push_back({key, value});
  }

  for (const auto& attr : pb_.resource().attributes()) {
    if (attr.column().can_be_json_encoded_array()) {
      resource_attributes_optional_json_encoded_.push_back(attr);
      continue;
    }
    resource_attributes_normal_encoding_.push_back(attr);
  }
  return Status::OK();
}

StatusOr<table_store::schema::Relation> OTelExportSinkOperator::OutputRelation(
    const table_store::schema::Schema&, const PlanState&, const std::vector<int64_t>&) const {
  DCHECK(is_initialized_) << "Not initialized";
  // There are no outputs.
  return table_store::schema::Relation();
}

}  // namespace plan
}  // namespace carnot
}  // namespace px
