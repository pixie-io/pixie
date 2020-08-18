#pragma once

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

StatusOr<ir::physical::Program> GeneratePhysicalProgram(
    const ir::logical::TracepointDeployment& input_program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
