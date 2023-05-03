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

#include <absl/container/flat_hash_map.h>

#include "proto/profile.pb.h"

namespace px {
namespace stirling {

using PProfProfile = ::perftools::profiles::Profile;
using histo_t = absl::flat_hash_map<std::string, uint64_t>;

// https://github.com/google/pprof/blob/main/proto/profile.proto
PProfProfile CreatePProfProfile(const uint32_t num_cpus, const histo_t& histo);

}  // namespace stirling
}  // namespace px
