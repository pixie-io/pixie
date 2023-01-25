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

#include "src/carnot/plan/plan_fragment.h"

#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/carnot/dag/dag.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace plan {

std::unique_ptr<PlanFragment> PlanFragment::FromProto(const planpb::PlanFragment& pb, int64_t id) {
  auto pf = std::make_unique<PlanFragment>(id);
  auto s = pf->Init(pb);
  // On init failure, return null;
  if (!s.ok()) {
    LOG(ERROR) << "Failed to initialize plan fragment";
    return nullptr;
  }
  return pf;
}

template <typename T, typename TWalkFunc>
Status PlanFragmentWalker::CallAs(const TWalkFunc& fn, const Operator& op) {
  DCHECK(fn) << "does not exist for op: " << op.DebugString();
  if (fn == nullptr) {
    return error::InvalidArgument("fn does not exist for op: $0", op.DebugString());
  }
  return fn(static_cast<const T&>(op));
}

Status PlanFragmentWalker::CallWalkFn(const Operator& op) {
  const auto op_type = op.op_type();
  switch (op_type) {
    case planpb::OperatorType::MEMORY_SOURCE_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<MemorySourceOperator>(on_memory_source_walk_fn_, op));
      break;
    case planpb::OperatorType::MAP_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<MapOperator>(on_map_walk_fn_, op));
      break;
    case planpb::OperatorType::AGGREGATE_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<AggregateOperator>(on_aggregate_walk_fn_, op));
      break;
    case planpb::OperatorType::MEMORY_SINK_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<MemorySinkOperator>(on_memory_sink_walk_fn_, op));
      break;
    case planpb::OperatorType::FILTER_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<FilterOperator>(on_filter_walk_fn_, op));
      break;
    case planpb::OperatorType::LIMIT_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<LimitOperator>(on_limit_walk_fn_, op));
      break;
    case planpb::OperatorType::JOIN_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<JoinOperator>(on_join_walk_fn_, op));
      break;
    case planpb::OperatorType::UNION_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<UnionOperator>(on_union_walk_fn_, op));
      break;
    case planpb::OperatorType::GRPC_SINK_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<GRPCSinkOperator>(on_grpc_sink_walk_fn_, op));
      break;
    case planpb::OperatorType::GRPC_SOURCE_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<GRPCSourceOperator>(on_grpc_source_walk_fn_, op));
      break;
    case planpb::OperatorType::UDTF_SOURCE_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<UDTFSourceOperator>(on_udtf_source_walk_fn_, op));
      break;
    case planpb::OperatorType::EMPTY_SOURCE_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<EmptySourceOperator>(on_empty_source_walk_fn_, op));
      break;
    case planpb::OperatorType::OTEL_EXPORT_SINK_OPERATOR:
      PX_RETURN_IF_ERROR(CallAs<OTelExportSinkOperator>(on_otel_sink_walk_fn_, op));
      break;
    default:
      LOG(FATAL) << absl::Substitute("Operator does not exist: $0", magic_enum::enum_name(op_type));
      return error::InvalidArgument("Operator does not exist: $0", magic_enum::enum_name(op_type));
  }
  return Status::OK();
}

Status PlanFragmentWalker::Walk(PlanFragment* plan_fragment) {
  auto operators = plan_fragment->dag().TopologicalSort();
  for (const auto& node_id : operators) {
    auto node = plan_fragment->nodes().find(node_id);
    if (node == plan_fragment->nodes().end()) {
      LOG(WARNING) << absl::Substitute("Could not find node $0 in plan fragment", node_id);
    } else {
      PX_RETURN_IF_ERROR(CallWalkFn(*node->second));
    }
  }
  return Status::OK();
}

}  // namespace plan
}  // namespace carnot
}  // namespace px
