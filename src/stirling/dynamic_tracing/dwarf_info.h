#pragma once

#include "src/common/base/base.h"
#include "src/stirling/proto/ir.pb.h"
#include "src/stirling/proto/physical_ir.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

StatusOr<dynamictracingpb::PhysicalProbe> AddDwarves(const dynamictracingpb::Probe& input_probe);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
