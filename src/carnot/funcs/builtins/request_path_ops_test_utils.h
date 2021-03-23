#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "src/carnot/funcs/builtins/request_path_ops.h"

MATCHER_P(HasCentroids, centroids, "") {
  std::vector<std::string> arg_centroids;
  for (const auto& cluster : arg.clusters()) {
    if (cluster.members().size() != 0) {
      for (const auto& req_path : cluster.members()) {
        arg_centroids.push_back(req_path.ToString());
      }
    } else {
      arg_centroids.push_back(cluster.centroid().ToString());
    }
  }
  *result_listener << "the centroids are " << ::testing::PrintToString(arg_centroids);
  return ::testing::Matches(::testing::UnorderedElementsAreArray(centroids))(arg_centroids);
}
