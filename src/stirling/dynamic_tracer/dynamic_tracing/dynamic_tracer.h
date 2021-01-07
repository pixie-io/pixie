#pragma once

#include "src/common/base/base.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/stirling/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"
#include "src/stirling/dynamic_tracer/dynamic_tracing/types.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

/**
 * Resolves the DeploymentSpec target into a path on the host.
 *  - Resolves PIDs to executable paths.
 *  - Resolves SharedObjects to shared object paths.
 *
 * Successfully calling this function result into deployment_spec->path being overwritten with
 * the path of an existing executable file or a shared object file.
 *
 * This must be called before calling CompileProgram().
 */
Status ResolveTargetObjPath(const md::K8sMetadataState& k8s_mds,
                            ir::shared::DeploymentSpec* deployment_spec);

/**
 * Transforms any logical probes inside a program into entry and return probes.
 * Also automatically adds any required supporting maps and implicit outputs.
 */
StatusOr<BCCProgram> CompileProgram(ir::logical::TracepointDeployment* input_program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
