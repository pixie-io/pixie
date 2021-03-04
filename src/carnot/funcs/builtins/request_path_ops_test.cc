#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <vector>

#include "src/carnot/funcs/builtins/request_path_ops.h"
#include "src/carnot/udf/test_utils.h"

namespace pl {
namespace carnot {
namespace builtins {

class RequestPathTest : public testing::TestWithParam<const char*> {};

TEST_P(RequestPathTest, serialization) {
  RequestPath path(GetParam());
  EXPECT_EQ(path, RequestPath::FromJSON(path.ToJSON()));
}

INSTANTIATE_TEST_SUITE_P(RequestPathVariants, RequestPathTest,
                         testing::Values("/abc/b/cedfeg12345", "/a/b?k=v?k2=v2",
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
  auto clustering = RequestPathClustering::FromJSON(serialized_clustering);
  ASSERT_EQ(1, clustering.clusters().size());
  EXPECT_EQ("/a/b/*", clustering.clusters()[0].centroid().ToString());
  EXPECT_EQ("/a/b/*", clustering.clusters()[0].Predict(RequestPath("/a/b/c")).ToString());
}

TEST(RequestPathClusteringFit, basic_low_cardinality) {
  auto uda_tester = udf::UDATester<RequestPathClusteringFitUDA>();

  auto serialized_clustering = uda_tester.ForInput("/a/b/d").ForInput("/a/b/c").Result();
  auto clustering = RequestPathClustering::FromJSON(serialized_clustering);
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

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
