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

#include "src/carnot/exec/ml/coreset.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

TEST(KMeansCoreset, basic) {
  Eigen::MatrixXf points(20, 5);
  points << Eigen::MatrixXf::Identity(5, 5), Eigen::MatrixXf::Identity(5, 5),
      Eigen::MatrixXf::Identity(5, 5), Eigen::MatrixXf::Identity(5, 5);
  auto weights = Eigen::MatrixXf::Ones(20, 1);

  auto coreset =
      KMeansCoreset::FromWeightedPointSet(std::make_shared<WeightedPointSet>(points, weights), 100);
  auto actual_points = coreset->points();
  // With a sample size of 100, the algorithm should output each of the unit vectors at least once,
  // with very very high probability. By my estimates it should succeed with probability at least
  // ~0.999999999.
  EXPECT_TRUE(
      (actual_points.colwise().sum().transpose().array() >= Eigen::VectorXf::Ones(5).array())
          .all());
}

TEST(CoresetDriver, basic) {
  // Create a coreset driver using the Coreset R-way tree data structure, and kmeans coresets.
  // Uses base buckets of size 64, points of size 64, 4-way tree, and coresets of size 64.
  int d = 64;
  CoresetDriver<CoresetTree<KMeansCoreset>> driver(64, d, 4, 64);
  Eigen::VectorXf point = Eigen::VectorXf::Random(d);
  // Insert 10 buckets worth of points.
  for (int i = 0; i < 64 * 10; i++) {
    driver.Update(point);
  }
  auto point_set = driver.Query();
  // After inserting 10 buckets, the first 8 should get merged into 2 buckets on the second level,
  // leaving 2 buckets on the second level, and the remaining 2 buckets on the first level.
  // Thus the query point set should have size 4 * bucket_size = 256.
  EXPECT_EQ(256, point_set->size());
}

TEST(CoresetDriver, merge) {
  int d = 64;
  CoresetDriver<CoresetTree<KMeansCoreset>> driver1(64, d, 4, 64);
  CoresetDriver<CoresetTree<KMeansCoreset>> driver2(64, d, 4, 64);
  Eigen::VectorXf point = Eigen::VectorXf::Random(d);
  // Insert 10 buckets of data into the first driver.
  for (int i = 0; i < 64 * 10; i++) {
    driver1.Update(point);
  }
  // Insert 8 buckets of data into the second driver.
  for (int i = 0; i < 64 * 8; i++) {
    driver2.Update(point);
  }
  // At this point, one tree should have 2 buckets in the second level and 2 in the first level, and
  // the other should have 2 buckets in the second level.
  driver1.Merge(driver2);
  // After merging, there should be 1 bucket in the 3rd level, and 2 buckets in the first level, so
  // the query set should have size 3 * bucket_size = 192.
  EXPECT_EQ(192, driver1.Query()->size());
}

TEST(CoresetDriver, serialization) {
  // Create a coreset driver using the Coreset R-way tree data structure, and kmeans coresets.
  // Uses base buckets of size 64, points of size 64, 4-way tree, and coresets of size 64.
  int d = 64;
  CoresetDriver<CoresetTree<KMeansCoreset>> driver(64, d, 4, 64);
  Eigen::VectorXf point = Eigen::VectorXf::Random(d);
  // Insert 10 buckets worth of points.
  for (int i = 0; i < 64 * 10; i++) {
    driver.Update(point);
  }
  // Serialize to json.
  auto serialized = driver.ToJSON();

  CoresetDriver<CoresetTree<KMeansCoreset>> driver2(64, d, 4, 64);
  driver2.FromJSON(serialized);

  // Should function as normal after serialize/deserialize
  auto point_set = driver2.Query();
  // After inserting 10 buckets, the first 8 should get merged into 2 buckets on the second level,
  // leaving 2 buckets on the second level, and the remaining 2 buckets on the first level.
  // Thus the query point set should have size 4 * bucket_size = 256.
  EXPECT_EQ(256, point_set->size());
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
