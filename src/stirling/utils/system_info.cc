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

#include "src/stirling/utils/system_info.h"

#include <sys/sysinfo.h>
#include <string>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_pid_path.h"

namespace px {
namespace system {

namespace {
void LogFileContents(std::filesystem::path file) {
  StatusOr<std::string> contents = ReadFileToString(file);
  if (contents.ok()) {
    LOG(INFO) << absl::Substitute("$0:\n$1", file.string(), contents.ConsumeValueOrDie());
  }
}
}  // namespace

void LogSystemInfo() {
  const system::Config& sysconfig = system::Config::GetInstance();
  LOG(INFO) << absl::StrCat("Location of proc: ", ProcPath().string());
  LOG(INFO) << absl::StrCat("Location of sysfs: ", sysconfig.sysfs_path().string());
  LOG(INFO) << absl::StrCat("Number of CPUs: ", get_nprocs_conf());

  // Log /proc/version.
  LogFileContents(ProcPath("version"));

  // Log /etc/*-release (e.g. /etc/lsb-release, /etc/os-release).
  const std::filesystem::path host_etc_path = sysconfig.ToHostPath("/etc");
  if (fs::Exists(host_etc_path)) {
    for (auto& p : std::filesystem::directory_iterator(host_etc_path)) {
      if (absl::EndsWith(p.path().string(), "-release")) {
        LogFileContents(p);
      }
    }
  }
}

}  // namespace system
}  // namespace px
