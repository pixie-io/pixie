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

#include <absl/strings/ascii.h>
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

void FindProbesAndInsertPaths(const std::string_view probe_str,
                              const dynamic_tracing::ir::shared::BinaryPathList& path_list,
                              std::string* script) {
  size_t paths_size = path_list.paths_size();

  size_t pos = 0;
  while ((pos = script->find(probe_str, pos)) != std::string::npos) {
    // BPFTrace uprobe script follows the following formats:
    // uprobe:<function_name>{...}
    // uprobe:<function_name>,uprobe:<function_name>{...}
    // Find the ending position of <function_name> by matching the next { or , character.
    size_t function_ending_pos = script->find_first_of("{,", pos);
    if (function_ending_pos == std::string::npos) {
      VLOG(1) << "Unable to find end of function name when inserting target object paths.";
      break;
    }

    // Get the function name.
    std::string function_name =
        script->substr(pos + probe_str.size(), function_ending_pos - pos - probe_str.size());
    absl::StripTrailingAsciiWhitespace(&function_name);
    const std::string_view function_name_strview(function_name);

    // For each path, insert the path and the function name form "uprobe:<path>:<function_name>".
    std::string uprobe_location;
    for (size_t i = 0; i < paths_size; ++i) {
      std::string ending_comma = (i == paths_size - 1) ? "" : ",\n";
      uprobe_location.append(
          absl::StrCat(probe_str, path_list.paths(i), ":", function_name_strview, ending_comma));
    }
    script->replace(pos, function_ending_pos - pos, uprobe_location);

    pos += uprobe_location.size();
  }
}

void InsertUprobeTargetObjPaths(const dynamic_tracing::ir::shared::DeploymentSpec& spec,
                                std::string* script) {
  constexpr std::string_view kUProbeStr("uprobe:");
  constexpr std::string_view kURetProbeStr("uretprobe:");

  size_t paths_size = spec.path_list().paths_size();
  // If path not specified, assume the user has put the path in the script already.
  if (paths_size == 0) {
    VLOG(1) << "No target binary path is provided when deploying BPFTrace Uprobe. No path is "
               "inserted after 'uprobe:' or 'uretprobe:'.";
    return;
  }

  FindProbesAndInsertPaths(kUProbeStr, spec.path_list(), script);
  FindProbesAndInsertPaths(kURetProbeStr, spec.path_list(), script);

  // TODO(chengruizhe): Add integration with StirlingError.
}

}  // namespace stirling
}  // namespace px
