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

#include "src/common/base/base.h"
#include "third_party/eigen3/Eigen/Core"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

void sample_from_probs(const Eigen::ArrayXf& probs, Eigen::ArrayXi* inds, size_t sample_size);

size_t randint(size_t high);

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
