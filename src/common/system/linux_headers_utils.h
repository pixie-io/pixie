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

#include "src/common/base/base.h"

namespace px {
namespace system {

constexpr std::string_view kLinuxModulesDir = "/lib/modules/";

/**
 * Resolves a possible symlink path to its corresponding host filesystem path.
 *
 * This function takes a filesystem path and checks if it is a symbolic link. If it is,
 * the symlink is resolved to its target path. Depending on whether the target is an absolute
 * or relative path, it is further processed to convert it into a valid host path (as in
 * Config::ToHostPath(...) path).
 *
 * If the input path is not a symlink, it is returned as-is. The function ensures that
 * the final resolved path exists in the host filesystem before returning it. Errors are
 * returned when the path does not exist, the resolution fails, or when there is an issue
 * accessing the filesystem.
 */
StatusOr<std::filesystem::path> ResolvePossibleSymlinkToHostPath(const std::filesystem::path p);

}  // namespace system
}  // namespace px
