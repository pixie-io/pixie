#pragma once

#include <memory>
#include <string>

#include "src/carnot/exec/ml/coreset.h"

namespace pl {
namespace carnot {
namespace exec {
namespace ml {

class KMeans {
 public:
  enum KMeansInitType {
    kKMeansPlusPlus = 0,
  };
  explicit KMeans(int k, int max_iters = 10, KMeansInitType init_type = kKMeansPlusPlus)
      : k_(k), max_iters_(max_iters), init_type_(init_type) {}

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
};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace pl
