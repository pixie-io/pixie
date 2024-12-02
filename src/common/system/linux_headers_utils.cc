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

#include "src/common/system/linux_headers_utils.h"

#include <fstream>
#include <limits>
#include <memory>
#include <string>

#include "src/common/base/file.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"

namespace px {
namespace system {

StatusOr<std::filesystem::path> ResolvePossibleSymlinkToHostPath(const std::filesystem::path p) {
  // Check if "p" is a symlink.
  std::error_code ec;
  const bool is_symlink = std::filesystem::is_symlink(p, ec);
  if (ec) {
    return error::NotFound(absl::Substitute("Did not find the host headers at path: $0, $1.",
                                            p.string(), ec.message()));
  }

  if (!is_symlink) {
    // Not a symlink, we are good now.
    return p;
  }

  // Resolve the symlink, and re-convert to a host path..
  const std::filesystem::path resolved = std::filesystem::read_symlink(p, ec);
  if (ec) {
    return error::Internal(ec.message());
  }

  // Relative paths containing "../" can result in an invalid host mount path when using
  // ToHostPath. Therefore, we need to treat the absolute and relative cases differently.
  std::filesystem::path resolved_host_path;
  if (resolved.is_absolute()) {
    resolved_host_path = system::Config::GetInstance().ToHostPath(resolved);
    VLOG(1) << absl::Substitute(
        "Symlink target is an absolute path. Converting that to host path: $0 -> $1.",
        resolved.string(), resolved_host_path.string());
  } else {
    resolved_host_path = p.parent_path();
    resolved_host_path /= resolved.string();
    VLOG(1) << absl::Substitute(
        "Symlink target is a relative path. Concatenating it to parent directory: $0",
        resolved_host_path.string());
  }

  // Downstream won't be ok unless the resolved host path exists; return an error if needed.
  if (!fs::Exists(resolved_host_path)) {
    return error::NotFound(absl::Substitute("Did not find host headers at resolved path: $0.",
                                            resolved_host_path.string()));
  }
  return resolved_host_path;
}

}  // namespace system
}  // namespace px
