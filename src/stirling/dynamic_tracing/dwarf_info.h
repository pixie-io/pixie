#pragma once

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

StatusOr<ir::physical::PhysicalProbe> AddDwarves(const ir::logical::Probe& input_probe);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
