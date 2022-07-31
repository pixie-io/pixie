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

#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/dynamic_bpftrace/utils.h"

namespace px {
namespace stirling {

bool ContainsUProbe(const std::string& script) {
  return absl::StrContains(script, "uprobe") || absl::StrContains(script, "uretprobe");
}

void InsertUprobeTargetObjPath(const dynamic_tracing::ir::shared::DeploymentSpec& spec,
                               std::string* script) {
  constexpr std::string_view kUProbeStr("uprobe:");
  constexpr std::string_view kURetProbeStr("uretprobe:");

  // TODO(chengruizhe): Add support for multiple deployment specs.
  // If path not specified, assume the user has put the path in the script already.
  if (spec.path().empty()) {
    VLOG(1) << "No target binary path is provided when deploying BPFTrace Uprobe. No path is "
               "inserted after 'uprobe:' or 'uretprobe:'.";
    return;
  }

  const std::string target = absl::StrCat(spec.path(), ":");
  const std::string_view target_strview(target);

  // Insert path after 'uprobe:' or 'uretprobe:'.
  *script =
      absl::StrReplaceAll(*script, {{kUProbeStr, absl::StrCat(kUProbeStr, target_strview)},
                                    {kURetProbeStr, absl::StrCat(kURetProbeStr, target_strview)}});

  // TODO(chengruizhe): Add integration with StirlingError.
}

}  // namespace stirling
}  // namespace px
