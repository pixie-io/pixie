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

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

#include "src/common/base/base.h"
#include "third_party/eigen3/Eigen/Core"

#include "src/carnot/exec/ml/sampling.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

void sample_from_probs(const Eigen::ArrayXf& probs, Eigen::ArrayXi* inds, size_t sample_size) {
  DCHECK_LT(pow(1.0f - probs.sum(), 2), 1e-4);
  inds->resize(sample_size);
  // cumsum for probs
  std::vector<float> cumsum(probs.size());
  std::partial_sum(probs.begin(), probs.end(), cumsum.begin());

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  for (auto i = 0UL; i < sample_size; i++) {
    auto it = std::lower_bound(cumsum.begin(), cumsum.end(), dis(gen));
    inds->operator()(i) = it - cumsum.begin();
  }
}

size_t randint(size_t high) { return rand() % high; }

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
