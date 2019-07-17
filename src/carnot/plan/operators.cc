#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/plan/utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace plan {

using pl::Status;

template <typename TOp, typename TProto>
std::unique_ptr<Operator> CreateOperator(int64_t id, const TProto& pb) {
  auto op = std::make_unique<TOp>(id);
  auto s = op->Init(pb);
  // On init failure, return null;
  if (!s.ok()) {
    LOG(ERROR) << "Failed to initialize operator";
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
    case planpb::BLOCKING_AGGREGATE_OPERATOR:
      return CreateOperator<BlockingAggregateOperator>(id, pb.blocking_agg_op());
    case planpb::MEMORY_SINK_OPERATOR:
      return CreateOperator<MemorySinkOperator>(id, pb.mem_sink_op());
    case planpb::FILTER_OPERATOR:
      return CreateOperator<FilterOperator>(id, pb.filter_op());
    case planpb::LIMIT_OPERATOR:
      return CreateOperator<LimitOperator>(id, pb.limit_op());
    case planpb::ZIP_OPERATOR:
      return CreateOperator<ZipOperator>(id, pb.zip_op());
    default:
      LOG(FATAL) << absl::Substitute("Unknown operator type: $0", ToString(pb.op_type()));
  }
}

/**
 * Memory Source Operator Implementation.
 */

std::string MemorySourceOperator::DebugString() const { return "Operator: MemorySource"; }

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
    PL_RETURN_IF_ERROR(s);
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
    PL_RETURN_IF_ERROR(s);
    r.AddColumn(s.ConsumeValueOrDie(), column_names_[idx]);
  }
  return r;
}

/**
 * Blocking Aggregate Operator Implementation.
 */

std::string BlockingAggregateOperator::DebugString() const { return "Operator: BlockingAggregate"; }

Status BlockingAggregateOperator::Init(const planpb::BlockingAggregateOperator& pb) {
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
    PL_RETURN_IF_ERROR(s);
    values_.emplace_back(std::unique_ptr<AggregateExpression>(std::move(ae)));
  }
  groups_.reserve(pb_.groups_size());
  for (int idx = 0; idx < pb_.groups_size(); ++idx) {
    groups_.emplace_back(GroupInfo{pb_.group_names(idx), pb_.groups(idx).index()});
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> BlockingAggregateOperator::OutputRelation(
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

  auto input_relation_s = schema.GetRelation(input_ids[0]);
  PL_RETURN_IF_ERROR(input_relation_s);
  const auto input_relation = input_relation_s.ConsumeValueOrDie();
  table_store::schema::Relation output_relation;

  for (int idx = 0; idx < pb_.groups_size(); ++idx) {
    int64_t node_id = pb_.groups(idx).node();
    int64_t col_idx = pb_.groups(idx).index();
    if (node_id != input_ids[0]) {
      return error::InvalidArgument("Column does not belong to the correct input node");
    }
    if (col_idx > static_cast<int64_t>(input_relation.NumColumns())) {
      return error::InvalidArgument("Column index is out of bounds");
    }
    output_relation.AddColumn(input_relation.GetColumnType(col_idx), pb_.group_names(idx));
  }

  for (const auto& value : values_) {
    auto s = value->OutputDataType(state, schema);
    PL_RETURN_IF_ERROR(s);
    output_relation.AddColumn(s.ConsumeValueOrDie(), value->name());
  }
  return output_relation;
}

/**
 * Memory Sink Operator Implementation.
 */

std::string MemorySinkOperator::DebugString() const { return "Operator: MemorySink"; }

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
 * Filter Operator Implementation.
 */
std::string FilterOperator::DebugString() const {
  std::string debug_string = absl::Substitute("($0)", expression_->DebugString());
  return "Op:Filter" + debug_string;
}

Status FilterOperator::Init(const planpb::FilterOperator& pb) {
  pb_ = pb;
  PL_ASSIGN_OR_RETURN(expression_, ScalarExpression::FromProto(pb_.expression()));

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

  auto input_relation_s = schema.GetRelation(input_ids[0]);
  PL_RETURN_IF_ERROR(input_relation_s);
  const auto input_relation = input_relation_s.ConsumeValueOrDie();

  // Output relation is the same as the input relation.
  return input_relation;
}

/**
 * Limit Operator Implementation.
 */
std::string LimitOperator::DebugString() const {
  std::string debug_string = absl::Substitute("($0)", record_limit_);
  return "Op:Limit" + debug_string;
}

Status LimitOperator::Init(const planpb::LimitOperator& pb) {
  pb_ = pb;
  record_limit_ = pb_.limit();

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

  auto input_relation_s = schema.GetRelation(input_ids[0]);
  PL_RETURN_IF_ERROR(input_relation_s);
  const auto input_relation = input_relation_s.ConsumeValueOrDie();

  // Output relation is the same as the input relation.
  return input_relation;
}

/**
 * Zip Operator Implementation.
 */
std::string ZipOperator::DebugString() const {
  return absl::Substitute("Op:Zip(columns=($0), time_columns=($1))",
                          absl::StrJoin(column_names_, ","),
                          absl::StrJoin(time_column_indexes_, ","));
}

Status ZipOperator::Init(const planpb::ZipOperator& pb) {
  pb_ = pb;

  column_names_.reserve(static_cast<size_t>(pb_.column_names_size()));
  for (int i = 0; i < pb_.column_names_size(); ++i) {
    column_names_.emplace_back(pb_.column_names(i));
  }

  column_mappings_.reserve(static_cast<size_t>(pb_.column_mappings_size()));
  bool has_time_column = false;

  for (int i = 0; i < pb_.column_mappings_size(); ++i) {
    if (i == 0) {
      has_time_column = pb_.column_mappings(i).has_time_column();
    } else if (has_time_column != pb_.column_mappings(i).has_time_column()) {
      return error::InvalidArgument(
          "Time column index must be set for either all tables or no tables.");
    }

    if (has_time_column) {
      time_column_indexes_.emplace_back(pb_.column_mappings(i).time_column_index());
    }

    std::vector<int64_t> mapping;
    for (int output_index : pb_.column_mappings(i).output_column_indexes()) {
      if (output_index < 0 || static_cast<size_t>(output_index) > column_names_.size()) {
        return error::InvalidArgument(
            "Output index $0 out of bounds for parent relation $1 of ZipOperator", output_index, i);
      }
      mapping.emplace_back(output_index);
    }
    column_mappings_.emplace_back(mapping);
  }

  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> ZipOperator::OutputRelation(
    const table_store::schema::Schema& schema, const PlanState& /*state*/,
    const std::vector<int64_t>& input_ids) const {
  DCHECK(is_initialized_) << "Not initialized";

  if (input_ids.size() != column_mappings_.size()) {
    return error::InvalidArgument("ZipOperator expected $0 input relations but received $1",
                                  column_mappings_.size(), input_ids.size());
  }

  // Keep track of the expected types for each output column.
  std::unordered_map<int64_t, types::DataType> type_map;

  // Parse each input and make sure the contents match what we expected to see.
  for (size_t i = 0; i < input_ids.size(); ++i) {
    if (!schema.HasRelation(input_ids[i])) {
      return error::NotFound("Missing relation ($0) for input of ZipOperator", input_ids[i]);
    }

    auto input_relation_s = schema.GetRelation(input_ids[i]);
    PL_RETURN_IF_ERROR(input_relation_s);
    PL_ASSIGN_OR_RETURN(const auto& input_relation, schema.GetRelation(input_ids[i]));

    for (size_t j = 0; j < column_mapping(i).size(); ++j) {
      auto output_index = column_mapping(i).at(j);      // Output index of column j
      auto col_type = input_relation.GetColumnType(j);  // Type of output column j

      // Add the column's type to the map if it isn't present so we can check for consistency.
      // Make sure that the types match for columns with the same output index.
      auto it = type_map.find(output_index);
      if (it != type_map.end() && it->second != col_type) {
        return error::InvalidArgument("Conflicting types for column ($0) in ZipOperator",
                                      column_names_[output_index]);
      }
      type_map.emplace(output_index, col_type);
    }
  }

  table_store::schema::Relation r;
  for (size_t i = 0; i < column_names_.size(); ++i) {
    auto found = type_map.find(i);
    if (found == type_map.end()) {
      return error::InvalidArgument("Output column $0 (name $1) of ZipOperator not defined", i,
                                    column_names_[i]);
    }
    r.AddColumn(found->second, column_names_[i]);
  }
  return r;
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
