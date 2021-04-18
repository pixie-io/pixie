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
