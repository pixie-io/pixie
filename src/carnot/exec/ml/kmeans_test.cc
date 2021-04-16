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
      cluster_inds.push_back(kmeans.Transform(points(20 * i + j, Eigen::all).transpose()));
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
