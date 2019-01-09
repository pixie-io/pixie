#include <memory>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace plan {

using pl::Status;

std::string ToString(planpb::OperatorType op) {
  switch (op) {
    case planpb::MEMORY_SOURCE_OPERATOR:
      return "MemorySourceOperator";
    case planpb::MAP_OPERATOR:
      return "MapOperator";
    case planpb::BLOCKING_AGGREGATE_OPERATOR:
      return "BlockingAggregateOperator";
    case planpb::MEMORY_SINK_OPERATOR:
      return "MemorySinkOperator";
    default:
      LOG(WARNING) << "Unknown operator in ToString function";
      return "UnknownOperator";
  }
}

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
  auto _ = pb;
  return Status::OK();
}

/**
 * Map Operator Implementation.
 */

std::string MapOperator::DebugString() { return "Operator: Map"; }

Status MapOperator::Init(const planpb::MapOperator& pb) {
  auto _ = pb;
  return Status::OK();
}

/**
 * Blocking Aggregate Operator Implementation.
 */

std::string BlockingAggregateOperator::DebugString() { return "Operator: BlockingAggregate"; }

Status BlockingAggregateOperator::Init(const planpb::BlockingAggregateOperator& pb) {
  auto _ = pb;
  return Status::OK();
}

/**
 * Memory Sink Operator Implementation.
 */

std::string MemorySinkOperator::DebugString() { return "Operator: MemorySink"; }

Status MemorySinkOperator::Init(const planpb::MemorySinkOperator& pb) {
  auto _ = pb;
  return Status::OK();
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
