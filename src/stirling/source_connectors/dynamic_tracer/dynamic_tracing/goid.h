#pragma once

#include <filesystem>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

// Returns a map used to record the mapping from pid_tgid to goid.
ir::shared::Map GenGOIDMap();

// Generates a probe for tracing goid.
ir::logical::Probe GenGOIDProbe();

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
