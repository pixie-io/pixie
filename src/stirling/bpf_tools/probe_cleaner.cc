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

#include <fcntl.h>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/probe_cleaner.h"

namespace px {
namespace stirling {
namespace utils {

const char kAttachedKProbesFile[] = "/sys/kernel/debug/tracing/kprobe_events";
const char kAttachedUProbesFile[] = "/sys/kernel/debug/tracing/uprobe_events";

Status SearchForAttachedProbes(const char* file_path, std::string_view marker,
                               std::vector<std::string>* leaked_probes) {
  std::ifstream infile(file_path);
  if (!infile.good()) {
    return error::Internal("Failed to open file for reading: $0", file_path);
  }
  std::string line;
  while (std::getline(infile, line)) {
    if (absl::StrContains(line, marker)) {
      std::vector<std::string> split = absl::StrSplit(line, ' ');
      if (split.size() != 2) {
        return error::Internal("Unexpected format when reading file: $0", file_path);
      }

      std::string probe = std::move(split[0]);

      // Note that a probe looks like the following:
      //     [p|r]:kprobes/your_favorite_probe_name_here __x64_sys_connect
      // Perform a quick (but not thorough) sanity check that we have the right format.
      // Detailed format: https://www.kernel.org/doc/html/latest/trace/kprobetrace.html.
      if (probe[0] != 'p' && probe[0] != 'r') {
        continue;
      }

      leaked_probes->push_back(std::move(probe));
    }
  }
  return Status::OK();
}

Status RemoveProbes(const char* file_path, std::vector<std::string> probes) {
  // Unfortunately std::ofstream doesn't properly append to /sys/kernel/debug/tracing/kprobe_events.
  // It appears related to its use of fopen() instead of open().
  // So doing the write in old-school C-style.
  int fd = open(file_path, O_WRONLY | O_APPEND, 0);
  if (fd < 0) {
    return error::Internal("Failed to open file for writing: $0", file_path);
  }

  std::vector<std::string> errors;
  for (auto& probe : probes) {
    std::vector<std::string_view> parts = absl::StrSplit(probe, absl::MaxSplits(':', 1));
    if (parts.size() != 2) {
      VLOG(1) << "Unexpected probe string format";
      continue;
    }

    std::string delete_probe = absl::StrCat("-:", parts[1]);
    VLOG(1) << absl::Substitute("Writing $0", delete_probe);

    if (write(fd, delete_probe.data(), delete_probe.size()) < 0) {
      VLOG(1) << absl::Substitute("Failed to write '$0' to file: $1 [errno=$2 message=$3]",
                                  delete_probe, file_path, errno, std::strerror(errno));
    }
    // Note that even if write succeeds, it doesn't confirm that the probe was properly removed.
    // We can only confirm that we wrote to the file.
  }

  close(fd);

  return Status::OK();
}

namespace {

Status CleanProbesFromSysFile(const char* file_path, std::string_view marker) {
  LOG(INFO) << absl::Substitute("Cleaning probes from $0 with the following marker: $1", file_path,
                                marker);

  std::vector<std::string> leaked_probes;
  PX_RETURN_IF_ERROR(SearchForAttachedProbes(file_path, marker, &leaked_probes));
  PX_RETURN_IF_ERROR(RemoveProbes(file_path, leaked_probes));

  std::vector<std::string> leaked_probes_after;
  PX_RETURN_IF_ERROR(SearchForAttachedProbes(file_path, marker, &leaked_probes_after));
  if (!leaked_probes_after.empty()) {
    return error::Internal(
        "Wasn't able to remove all Stirling probes: "
        "initial_count=$0, final_count=$1",
        leaked_probes.size(), leaked_probes_after.size());
  }
  LOG(INFO) << absl::Substitute("All Stirling probes removed (count=$0)", leaked_probes.size());

  return Status::OK();
}

}  // namespace

Status CleanProbes(std::string_view marker) {
  PX_RETURN_IF_ERROR(CleanProbesFromSysFile(kAttachedKProbesFile, marker));
  PX_RETURN_IF_ERROR(CleanProbesFromSysFile(kAttachedUProbesFile, marker));
  return Status::OK();
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
