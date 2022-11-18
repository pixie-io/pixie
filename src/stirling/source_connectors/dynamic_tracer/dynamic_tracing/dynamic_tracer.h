/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "src/common/base/base.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/types.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

/**
 * Resolves the DeploymentSpec target into paths on the host.
 *  - Resolves PIDs to executable paths.
 *  - Resolves SharedObjects to shared object paths.
 *
 * Successfully calling this function result into deployment_spec->paths being overwritten with
 * the paths of existing executable files or shared object files.
 *
 * This must be called before calling CompileProgram().
 */
Status ResolveTargetObjPaths(const md::K8sMetadataState& k8s_mds,
                             ir::shared::DeploymentSpec* deployment_spec);

/**
 * Transforms any logical probes inside a program into entry and return probes.
 * Also automatically adds any required supporting maps and implicit outputs.
 */
StatusOr<BCCProgram> CompileProgram(ir::logical::TracepointDeployment* input_program);

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
