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

#include <memory>

#include "src/carnot/exec/ml/coreset.h"
#include "src/carnot/exec/ml/sampling.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

void KMeansCoreset::Construct(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights) {
  auto weight_sum = weights.sum();
  auto weighted_mean = ((weights.transpose() * points) / weight_sum).eval();
  auto dists = (points.rowwise() - weighted_mean).rowwise().squaredNorm().eval();
  auto weighted_dists = (weights.array() * dists.array()).matrix().eval();
  auto weighted_dists_sum = weighted_dists.sum();

  // q(x)
  auto probs =
      ((0.5 * weights / weight_sum).array() + 0.5 * (weighted_dists / weighted_dists_sum).array())
          .eval();
  Eigen::ArrayXi sample_inds;
  sample_from_probs(probs, &sample_inds, size_);
  points_ = points(sample_inds, Eigen::indexing::all);
  // u(x)
  weights_ = weights(sample_inds, Eigen::indexing::all).array() /
             (size_ * probs(sample_inds, Eigen::indexing::all)).array();
}

std::shared_ptr<KMeansCoreset> KMeansCoreset::FromWeightedPointSet(
    std::shared_ptr<WeightedPointSet> set, size_t coreset_size) {
  auto coreset = std::make_shared<KMeansCoreset>(coreset_size, set->point_size());
  coreset->Construct(set->points(), set->weights());
  return coreset;
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
