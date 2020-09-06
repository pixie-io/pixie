#pragma once

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"
#include "src/stirling/dynamic_tracing/types.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

/**
 * Transforms any logical probes inside a program into entry and return probes.
 * Also automatically adds any required supporting maps and implicit outputs.
 */
StatusOr<BCCProgram> CompileProgram(const ir::logical::TracepointDeployment& input_program);

/**
 * Resolves the DeploymentSpec target into a path on the host.
 *  - Resolves PIDs to executable paths.
 *  - Resolves SharedObjects to shared object paths.
 */
Status ResolveTargetObjPath(ir::shared::DeploymentSpec* deployment_spec);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
