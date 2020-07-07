#pragma once

#include <filesystem>

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

// Generates a probe and its map for tracing goid.
// TODO(yzhao): Call this in the process of intermediate to physical translation.
void GenGOIDEntryProbe(ir::logical::Program* program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
