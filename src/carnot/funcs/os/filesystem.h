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

#include <filesystem>
#include <string>
#include <vector>

#include "src/carnot/funcs/shared/utils.h"
#include "src/common/system/proc_pid_path.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace funcs {
namespace os {
namespace internal {

inline std::string GetSharedLibraries(md::UPID upid) {
  auto pid = upid.pid();
  std::vector<std::string> libraries;
  const auto fpath = system::ProcPidPath(pid, "map_files");
  if (!std::filesystem::exists(fpath)) {
    return "";
  }
  for (const auto& p : std::filesystem::directory_iterator(fpath)) {
    if (std::filesystem::is_symlink(p)) {
      auto symlink = std::filesystem::read_symlink(p);
      libraries.push_back(symlink.u8string());
    }
  }

  return StringifyVector(libraries);
}

}  // namespace internal

}  // namespace os
}  // namespace funcs
}  // namespace carnot
}  // namespace px
