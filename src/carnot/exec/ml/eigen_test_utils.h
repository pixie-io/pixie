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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "third_party/eigen3/Eigen/Core"

MATCHER_P2(IsApproxMatrix, matrix, tol, "") {
  return matrix.isApprox(arg, static_cast<float>(tol));
}

MATCHER_P2(UnorderedRowsAre, matrix, tol, "") {
  std::vector<::testing::Matcher<Eigen::MatrixXf>> rows;
  for (int i = 0; i < matrix.rows(); i++) {
    rows.push_back(IsApproxMatrix(matrix(i, Eigen::indexing::all), tol));
  }
  std::vector<Eigen::MatrixXf> arg_rows;
  for (int i = 0; i < arg.rows(); i++) {
    arg_rows.push_back(arg(i, Eigen::indexing::all));
  }
  return ::testing::Matches(::testing::UnorderedElementsAreArray(rows))(arg_rows);
}

Eigen::MatrixXf kmeans_test_data();
Eigen::MatrixXf kmeans_expected_centroids();
