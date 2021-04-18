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
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace utils {

static constexpr char kPixieBPFProbeMarker[] = "__pixie__";

/**
 * Reads sysfs to create a list of currently deployed probes with the specified marker in the name.
 *
 * Stirling probes are determined by a special marker in the probe name,
 * so this function is primarily used to search for leaked Stirling probes.
 *
 * @param marker a marker that must be in the probe name for it be returned.
 *
 * @return vector of probe names.
 */
Status SearchForAttachedProbes(const char* file_path, std::string_view marker,
                               std::vector<std::string>* leaked_probes);

/**
 * Removes the specified probes using the provided names.
 *
 * Note that this does not confirm that the probes exist,
 * or even that they were successfully removed.
 * It simply issues the commands to the sysfs file to remove the probes.
 *
 * @param probes vector of probes to remove.
 * @return error if there were issues accessing sysfs.
 */
Status RemoveProbes(const char* file_path, std::vector<std::string> probes);

/**
 * Searches for and removes any kprobe with the specified marker.
 *
 * @param marker a marker that must be in the probe name for it be returned.
 *
 * @return error if issues accessing sysfs, or if there are still probes leftover after the cleanup
 * process.
 */
Status CleanProbes(std::string_view marker = kPixieBPFProbeMarker);

}  // namespace utils
}  // namespace stirling
}  // namespace px
