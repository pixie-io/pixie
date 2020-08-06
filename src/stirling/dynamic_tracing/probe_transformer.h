#pragma once

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

/**
 * Transforms any logical probes inside a program into entry and return probes.
 * Also automatically adds any required supporting maps and implicit outputs.
 */
StatusOr<ir::logical::TracepointDeployment> TransformLogicalProgram(
    const ir::logical::TracepointDeployment& input_program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
