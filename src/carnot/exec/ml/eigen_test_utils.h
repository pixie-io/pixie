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
    rows.push_back(IsApproxMatrix(matrix(i, Eigen::all), tol));
  }
  std::vector<Eigen::MatrixXf> arg_rows;
  for (int i = 0; i < arg.rows(); i++) {
    arg_rows.push_back(arg(i, Eigen::all));
  }
  return ::testing::Matches(::testing::UnorderedElementsAreArray(rows))(arg_rows);
}

Eigen::MatrixXf kmeans_test_data();
Eigen::MatrixXf kmeans_expected_centroids();
