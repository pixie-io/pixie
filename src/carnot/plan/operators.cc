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
    case planpb::AGGREGATE_OPERATOR:
      return CreateOperator<AggregateOperator>(id, pb.agg_op());
    case planpb::MEMORY_SINK_OPERATOR:
      return CreateOperator<MemorySinkOperator>(id, pb.mem_sink_op());
    case planpb::GRPC_SOURCE_OPERATOR:
      return CreateOperator<GrpcSourceOperator>(id, pb.grpc_source_op());
    case planpb::GRPC_SINK_OPERATOR:
      return CreateOperator<GrpcSinkOperator>(id, pb.grpc_sink_op());
    case planpb::FILTER_OPERATOR:
      return CreateOperator<FilterOperator>(id, pb.filter_op());
    case planpb::LIMIT_OPERATOR:
      return CreateOperator<LimitOperator>(id, pb.limit_op());
    case planpb::UNION_OPERATOR:
      return CreateOperator<UnionOperator>(id, pb.union_op());
    case planpb::JOIN_OPERATOR:
      return CreateOperator<JoinOperator>(id, pb.join_op());
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
 * Aggregate Operator Implementation.
 */

std::string AggregateOperator::DebugString() const { return "Operator: Aggregate"; }

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
 * GRPC Source Operator Implementation.
 */

std::string GrpcSourceOperator::DebugString() const {
  return absl::Substitute("Op:GrpcSource($0)", absl::StrJoin(pb_.column_names(), ","));
}

Status GrpcSourceOperator::Init(const planpb::GrpcSourceOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> GrpcSourceOperator::OutputRelation(
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

std::string GrpcSinkOperator::DebugString() const {
  return absl::Substitute("Op:GrpcSink($0, $1)", address(), destination_id());
}

Status GrpcSinkOperator::Init(const planpb::GrpcSinkOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<table_store::schema::Relation> GrpcSinkOperator::OutputRelation(
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
std::string UnionOperator::DebugString() const {
  return absl::Substitute("Op:Zip(columns=($0), time_columns=($1))",
                          absl::StrJoin(column_names_, ","),
                          absl::StrJoin(time_column_indexes_, ","));
}

Status UnionOperator::Init(const planpb::UnionOperator& pb) {
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
    }
    if (has_time_column != pb_.column_mappings(i).has_time_column()) {
      return error::InvalidArgument(
          "Time column index must be set for either all tables or no tables.");
    }
    if (has_time_column) {
      time_column_indexes_.emplace_back(pb_.column_mappings(i).time_column_index());
    }
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

    auto input_relation_s = schema.GetRelation(input_ids[i]);
    PL_RETURN_IF_ERROR(input_relation_s);
    PL_ASSIGN_OR_RETURN(const auto& input_relation, schema.GetRelation(input_ids[i]));

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

/**
 * Join Operator Implementation.
 */

std::string JoinOperator::DebugString(planpb::JoinOperator::JoinType type) {
  switch (type) {
    case planpb::JoinOperator::INNER:
      return "inner";
    case planpb::JoinOperator::LEFT_OUTER:
      return "left outer";
    case planpb::JoinOperator::FULL_OUTER:
      return "full outer";
    default:
      LOG(ERROR) << absl::Substitute("Unknown Join Type: $0", static_cast<int>(type));
      return absl::Substitute("UnknownJoinType:$0", static_cast<int>(type));
  }
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

  PL_ASSIGN_OR_RETURN(const auto& left_relation, schema.GetRelation(input_ids[0]));
  PL_ASSIGN_OR_RETURN(const auto& right_relation, schema.GetRelation(input_ids[1]));

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

bool JoinOperator::order_by_time() const {
  return std::find(column_names().begin(), column_names().end(), "time_") != column_names().end();
}

planpb::JoinOperator::ParentColumn JoinOperator::time_column() const {
  DCHECK(order_by_time());
  auto pos = std::distance(column_names().begin(),
                           std::find(column_names().begin(), column_names().end(), "time_"));
  return output_columns()[pos];
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
