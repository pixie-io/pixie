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
#include <vector>

#include "src/carnot/funcs/builtins/request_path_ops.h"
#include "src/carnot/funcs/builtins/request_path_ops_test_utils.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace builtins {

class RequestPathTest : public ::testing::TestWithParam<const char*> {};

TEST_P(RequestPathTest, serialization) {
  RequestPath path(GetParam());
  EXPECT_OK_AND_EQ(RequestPath::FromJSON(path.ToJSON()), path);
}

INSTANTIATE_TEST_SUITE_P(RequestPathVariants, RequestPathTest,
                         ::testing::Values("/abc/b/cedfeg12345", "/a/b?k=v?k2=v2",
                                           "/a/%2F/c/?k=v%2F1"));

TEST(RequestPathClusteringFit, basic) {
  auto uda_tester = udf::UDATester<RequestPathClusteringFitUDA>();

  auto serialized_clustering = uda_tester.ForInput("/a/b/c")
                                   .ForInput("/a/b/d")
                                   .ForInput("a/b/a")
                                   .ForInput("/a/b/b")
                                   .ForInput("/a/b/e")
                                   .ForInput("a/b/f")
                                   .Result();
  auto clustering_or_s = RequestPathClustering::FromJSON(serialized_clustering);
  ASSERT_OK(clustering_or_s);
  auto clustering = clustering_or_s.ConsumeValueOrDie();
  ASSERT_EQ(1, clustering.clusters().size());
  EXPECT_EQ("/a/b/*", clustering.clusters()[0].centroid().ToString());
  EXPECT_EQ("/a/b/*", clustering.clusters()[0].Predict(RequestPath("/a/b/c")).ToString());
}

TEST(RequestPathClusteringFit, basic_low_cardinality) {
  auto uda_tester = udf::UDATester<RequestPathClusteringFitUDA>();

  auto serialized_clustering = uda_tester.ForInput("/a/b/d").ForInput("/a/b/c").Result();
  auto clustering_or_s = RequestPathClustering::FromJSON(serialized_clustering);
  ASSERT_OK(clustering_or_s);
  auto clustering = clustering_or_s.ConsumeValueOrDie();
  ASSERT_EQ(1, clustering.clusters().size());
  EXPECT_EQ("/a/b/*", clustering.clusters()[0].centroid().ToString());
  EXPECT_EQ("/a/b/d", clustering.clusters()[0].Predict(RequestPath("/a/b/d")).ToString());
}

TEST(RequestPathClusteringPredict, basic) {
  auto uda_tester = udf::UDATester<RequestPathClusteringFitUDA>();
  auto serialized_clustering = uda_tester.ForInput("/a/b/c")
                                   .ForInput("/a/b/d")
                                   .ForInput("a/b/a")
                                   .ForInput("/a/b/b")
                                   .ForInput("/a/b/e")
                                   .ForInput("a/b/f")
                                   .Result();
  auto udf_tester = udf::UDFTester<RequestPathClusteringPredictUDF>();
  udf_tester.ForInput("/a/b/c", serialized_clustering).Expect("/a/b/*");
}

TEST(RequestPathClusteringPredict, basic_low_cardinality) {
  auto uda_tester = udf::UDATester<RequestPathClusteringFitUDA>();
  auto serialized_clustering = uda_tester.ForInput("/a/b/d").ForInput("/a/b/c").Result();

  auto udf_tester = udf::UDFTester<RequestPathClusteringPredictUDF>();
  udf_tester.ForInput("/a/b/c", serialized_clustering).Expect("/a/b/c");
}

TEST(RequestPathEndpointMatcher, basic) {
  auto udf_tester = udf::UDFTester<RequestPathEndpointMatcherUDF>();
  udf_tester.ForInput("/a/b/c", "/a/b/*").Expect(true);
  udf_tester.ForInput("/a/b/*", "/a/b/c").Expect(false);
  udf_tester.ForInput("/a/c/c", "/a/b/*").Expect(false);
}

// This tests the case where different PEMs have different clusterings of their own data, such that
// at merge time some of the individual points in not yet fully formed clusters on one PEM should've
// been clustered into one of the clusters on the other PEM. This should be handled by the logic in
// RequestPathClustering::Merge.
TEST(RequestPathClusteringFit, merging_clusters) {
  auto uda_tester1 = udf::UDATester<RequestPathClusteringFitUDA>();

  // PEM1 has a /a/b/* cluster. PEM2 has a /*/b/c cluster. PEM2's cluster doesn't meet the minimum
  // cardinality requirement, so its individual members should be checked to see if they fit in the
  // /a/b/* cluster. In this case /a/b/c fits into /a/b/*, so the final cluster centroids should be
  // /a/b*, /b/b/c, /c/b/c.
  uda_tester1.ForInput("/a/b/a")
      .ForInput("/a/b/b")
      .ForInput("/a/b/c")
      .ForInput("/a/b/d")
      .ForInput("/a/b/e")
      .ForInput("/a/b/f");

  auto uda_tester2 = udf::UDATester<RequestPathClusteringFitUDA>();
  uda_tester2.ForInput("/a/b/c").ForInput("/b/b/c").ForInput("/c/b/c");

  // Check that merging works in either order.
  auto serialized_clustering1 = uda_tester1.Merge(&uda_tester2).Result();
  auto serialized_clustering2 = uda_tester2.Merge(&uda_tester1).Result();
  auto clustering1_or_s = RequestPathClustering::FromJSON(serialized_clustering1);
  ASSERT_OK(clustering1_or_s);
  auto clustering1 = clustering1_or_s.ConsumeValueOrDie();
  auto clustering2_or_s = RequestPathClustering::FromJSON(serialized_clustering2);
  ASSERT_OK(clustering2_or_s);
  auto clustering2 = clustering2_or_s.ConsumeValueOrDie();

  std::vector<std::string> centroids({"/a/b/*", "/b/b/c", "/c/b/c"});
  EXPECT_THAT(clustering1, HasCentroids(centroids));
  EXPECT_THAT(clustering2, HasCentroids(centroids));
}

// More complicated scenario but similar situation to previous test.
TEST(RequestPathClusteringFit, merging_clusters_complicated) {
  std::array<udf::UDATester<RequestPathClusteringFitUDA>, 5> pems;
  // 4 PEMs, 4 of them have fully formed clusters, 1 of them has clusters that haven't reached the
  // minimum cardinality.

  pems[0]
      .ForInput("/prop1/0/prop2/0/prop3/0")
      .ForInput("/prop1/0/prop2/0/prop3/1")
      .ForInput("/prop1/0/prop2/0/prop3/2")
      .ForInput("/prop1/0/prop2/0/prop3/3")
      .ForInput("/prop1/0/prop2/0/prop3/4")
      .ForInput("/prop1/0/prop2/0/prop3/5");

  pems[1]
      .ForInput("/prop1/0/prop2/0/prop3/0")
      .ForInput("/prop1/0/prop2/1/prop3/0")
      .ForInput("/prop1/0/prop2/2/prop3/0")
      .ForInput("/prop1/0/prop2/3/prop3/0")
      .ForInput("/prop1/0/prop2/4/prop3/0")
      .ForInput("/prop1/0/prop2/5/prop3/0");

  pems[2]
      .ForInput("/prop1/0/prop2/0/prop3/0")
      .ForInput("/prop1/1/prop2/0/prop3/0")
      .ForInput("/prop1/2/prop2/0/prop3/0")
      .ForInput("/prop1/3/prop2/0/prop3/0")
      .ForInput("/prop1/4/prop2/0/prop3/0")
      .ForInput("/prop1/5/prop2/0/prop3/0");

  pems[3]
      .ForInput("/prop1/0/prop2/0/prop3/0")
      .ForInput("/prop1/1/prop2/1/prop3/0")
      .ForInput("/prop1/2/prop2/2/prop3/0")
      .ForInput("/prop1/3/prop2/3/prop3/0")
      .ForInput("/prop1/4/prop2/4/prop3/0")
      .ForInput("/prop1/5/prop2/5/prop3/0");

  pems[4]
      .ForInput("/prop1/0/prop2/0/prop3/a")
      .ForInput("/prop1/0/prop2/4/prop3/a")
      .ForInput("/prop1/0/prop2/5/prop3/a");

  std::vector<std::string> expected_centroids({"/prop1/*/prop2/*/prop3/*"});

  std::vector<int> permutation_indices({0, 1, 2, 3, 4});
  do {
    auto merged = udf::UDATester<RequestPathClusteringFitUDA>();
    for (const auto idx : permutation_indices) {
      merged.Merge(&pems[idx]);
    }
    auto serialized_clustering = merged.Result();
    auto clustering_or_s = RequestPathClustering::FromJSON(serialized_clustering);
    ASSERT_OK(clustering_or_s);
    auto clustering = clustering_or_s.ConsumeValueOrDie();
    EXPECT_THAT(clustering, HasCentroids(expected_centroids));
  } while (std::next_permutation(permutation_indices.begin(), permutation_indices.end()));
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
