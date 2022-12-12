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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"

// This file deals with process path resolution.
// In particular, FilePathResolver handles cases when these paths are within containers.

namespace px {
namespace stirling {

using MountInfoVec = std::vector<system::ProcParser::MountInfo>;

/**
 * Return the path to the currently running process (i.e. /proc/self/exe).
 * This function will return a host relative path self is in a container.
 */
StatusOr<std::filesystem::path> GetSelfPath();

}  // namespace stirling
}  // namespace px
