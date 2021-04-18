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

#include <memory>
#include <random>
#include <string>

#include "src/carnot/exec/ml/coreset.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

class KMeans {
 public:
  enum KMeansInitType {
    kKMeansPlusPlus = 0,
  };
  explicit KMeans(int k, int max_iters = 10, KMeansInitType init_type = kKMeansPlusPlus,
                  unsigned int seed = 42)
      : k_(k), max_iters_(max_iters), init_type_(init_type), random_gen_(seed) {}

  /**
   * Run kmeans on a weighted set of points.
   * Updates centroids_ based on running kmeans on this set.
   * Note that only the last call to Fit matters, eg. Fit(set1); Fit(set2); is equivalent to
   * Fit(set2).
   **/
  void Fit(std::shared_ptr<WeightedPointSet> set);

  /**
   * Transform returns the index of the centroid closest to point.
   **/
  size_t Transform(const Eigen::VectorXf& point);

  const Eigen::MatrixXf& centroids() const { return centroids_; }

  std::string ToJSON();
  void FromJSON(std::string data);

 private:
  bool LloydsIteration(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights);
  void KMeansPlusPlusInit(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights);

  int k_;
  int max_iters_;
  KMeansInitType init_type_;
  Eigen::MatrixXf centroids_;
  std::mt19937 random_gen_;
};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
