#pragma once

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

/**
 * Transforms a single probe of type LOGICAL into a program with
 * entry probes, return probes, maps and outputs.
 */
// TODO(oazizi): Support a mode that ir::logical::Program as input.
StatusOr<ir::logical::Program> TransformLogicalProbe(const ir::logical::Probe& input_probe);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
