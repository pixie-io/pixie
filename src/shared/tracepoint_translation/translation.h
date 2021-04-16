#pragma once

#include "src/carnot/planner/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace px {
namespace tracepoint {

void ConvertPlannerTracepointToStirlingTracepoint(
    const carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment& in,
    stirling::dynamic_tracing::ir::logical::TracepointDeployment* out);

}  // namespace tracepoint
}  // namespace px
