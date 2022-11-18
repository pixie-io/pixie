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

#include <string>
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/sharedpb/shared.pb.h"

namespace px {
namespace stirling {

// Checks if a BPFTrace script contains uprobes or uretprobes.
bool ContainsUProbe(const std::string& script);

// Inserts the path from the deployment spec after every occurance of `uprobe:` or `uretprobe:`.
void InsertUprobeTargetObjPaths(const dynamic_tracing::ir::shared::DeploymentSpec& spec,
                                std::string* script);
}  // namespace stirling
}  // namespace px
