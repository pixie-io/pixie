#include <glog/logging.h>
#include <memory>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/proto/plan.pb.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/plan/schema.h"
#include "src/carnot/plan/utils.h"
#include "src/utils/error.h"

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
    default:
      LOG(FATAL) << absl::StrFormat("Unknown operator type: %s", ToString(pb.op_type()));
  }
}

/**
 * Memory Source Operator Implementation.
 */

std::string MemorySourceOperator::DebugString() { return "Operator: MemorySource"; }

Status MemorySourceOperator::Init(const planpb::MemorySourceOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}
StatusOr<Relation> MemorySourceOperator::OutputRelation(const Schema&, const CompilerState&,
                                                        const std::vector<uint64_t>& input_ids) {
  DCHECK(is_initialized_) << "Not initialized";
  if (input_ids.size() != 0) {
    // TODO(zasgar): We should figure out if we need to treat the "source table" as
    // an input relation.
    return error::InvalidArgument("Source operator cannot have any inputs");
  }
  Relation r;
  for (int i = 0; i < pb_.column_idxs_size(); ++i) {
    r.AddColumn(pb_.column_types(i), pb_.column_names(i));
  }
  return StatusOr<Relation>();
}

/**
 * Map Operator Implementation.
 */

std::string MapOperator::DebugString() {
  std::string debug_string;
  debug_string += "(";
  for (size_t i = 0; i < expressions_.size(); ++i) {
    if (i) {
      debug_string += ",";
    }
    debug_string += absl::StrFormat("%s:%s", column_names_[i], expressions_[i]->DebugString());
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

StatusOr<Relation> MapOperator::OutputRelation(const Schema& schema, const CompilerState& state,
                                               const std::vector<uint64_t>& input_ids) {
  DCHECK(is_initialized_) << "Not initialized";
  if (input_ids.size() != 1) {
    return error::InvalidArgument("Map operator must have exactly one input");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing relation ($0) for input of Map", input_ids[0]);
  }
  Relation r;
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

std::string BlockingAggregateOperator::DebugString() { return "Operator: BlockingAggregate"; }

Status BlockingAggregateOperator::Init(const planpb::BlockingAggregateOperator& pb) {
  pb_ = pb;
  if (pb_.groups_size() != pb_.group_names_size()) {
    return error::InvalidArgument("group names/exp size mismatch");
  }
  if (pb_.values_size() != pb_.value_names_size()) {
    return error::InvalidArgument("values names/exp size mismatch");
  }
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<Relation> BlockingAggregateOperator::OutputRelation(
    const Schema& schema, const CompilerState&, const std::vector<uint64_t>& input_ids) {
  DCHECK(is_initialized_) << "Not initialized";
  if (input_ids.size() != 1) {
    return error::InvalidArgument("BlockingAgg operator must have exactly one input");
  }
  if (!schema.HasRelation(input_ids[0])) {
    return error::NotFound("Missing relation ($0) for input of BlockingAggregateOperator",
                           input_ids[0]);
  }

  return error::Unimplemented("Not implemented yet");
}

/**
 * Memory Sink Operator Implementation.
 */

std::string MemorySinkOperator::DebugString() { return "Operator: MemorySink"; }

Status MemorySinkOperator::Init(const planpb::MemorySinkOperator& pb) {
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}
StatusOr<Relation> MemorySinkOperator::OutputRelation(const Schema&, const CompilerState&,
                                                      const std::vector<uint64_t>&) {
  DCHECK(is_initialized_) << "Not initialized";
  // There are no outputs.
  return Relation();
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
