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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <vector>

#include "src/carnot/exec/ml/coreset.h"
#include "src/carnot/exec/ml/eigen_test_utils.h"
#include "src/carnot/exec/ml/kmeans.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

TEST(KMeans, basic) {
  int k = 5;
  Eigen::MatrixXf points(k, k);
  points << Eigen::MatrixXf::Identity(k, k);
  auto weights = Eigen::VectorXf::Ones(k);

  auto set = std::make_shared<WeightedPointSet>(points, weights);

  KMeans kmeans(k);

  kmeans.Fit(set);

  // In this simple case the centroids should be the input points.
  EXPECT_THAT(kmeans.centroids(), UnorderedRowsAre(points, 1e-6f));
}

TEST(KMeans, serialize_deserialize) {
  int k = 5;
  Eigen::MatrixXf points(k, k);
  points << Eigen::MatrixXf::Identity(k, k);
  auto weights = Eigen::VectorXf::Ones(k);

  auto set = std::make_shared<WeightedPointSet>(points, weights);

  KMeans kmeans(k);

  kmeans.Fit(set);

  auto serialized = kmeans.ToJSON();

  KMeans kmeans2(k);
  kmeans2.FromJSON(serialized);

  // In this simple case the centroids should be the input points.
  EXPECT_THAT(kmeans2.centroids(), UnorderedRowsAre(points, 1e-6f));
}

TEST(KMeans, trimodal_normal_dist) {
  int k = 3;

  Eigen::MatrixXf points = kmeans_test_data();
  Eigen::VectorXf weights = Eigen::VectorXf::Ones(60);

  Eigen::MatrixXf expected_centroids = kmeans_expected_centroids();

  auto set = std::make_shared<WeightedPointSet>(points, weights);

  KMeans kmeans(k);
  kmeans.Fit(set);

  ASSERT_THAT(kmeans.centroids(), UnorderedRowsAre(expected_centroids, 0.15));

  std::set<size_t> taken_inds;
  for (int i = 0; i < 3; i++) {
    std::vector<size_t> cluster_inds;
    for (int j = 0; j < 20; j++) {
      cluster_inds.push_back(
          kmeans.Transform(points(20 * i + j, Eigen::indexing::all).transpose()));
    }
    EXPECT_EQ(taken_inds.find(cluster_inds[0]), taken_inds.end());
    EXPECT_THAT(cluster_inds, ::testing::Each(cluster_inds[0]));
    taken_inds.insert(cluster_inds[0]);
  }
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
