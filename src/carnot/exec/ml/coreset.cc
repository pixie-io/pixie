#include <memory>

#include "src/carnot/exec/ml/coreset.h"
#include "src/carnot/exec/ml/sampling.h"

namespace pl {
namespace carnot {
namespace exec {
namespace ml {

void KMeansCoreset::Construct(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights) {
  auto weight_sum = weights.sum();
  auto weighted_mean = ((weights.transpose() * points) / weight_sum).eval();
  auto dists = (points.rowwise() - weighted_mean).rowwise().squaredNorm().eval();
  auto weighted_dists = (weights.array() * dists.array()).matrix().eval();
  auto weighted_dists_sum = weighted_dists.sum();

  // q(x)
  auto probs =
      ((0.5 * weights / weight_sum).array() + 0.5 * (weighted_dists / weighted_dists_sum).array())
          .eval();
  Eigen::ArrayXi sample_inds;
  sample_from_probs(probs, &sample_inds, set_size_);
  set_ = points(sample_inds, Eigen::all);
  // u(x)
  weights_ = weights(sample_inds, Eigen::all).array() /
             (set_size_ * probs(sample_inds, Eigen::all)).array();
  size_ = set_size_;
}

std::shared_ptr<KMeansCoreset> KMeansCoreset::FromWeightedPointSet(
    std::shared_ptr<WeightedPointSet> set, size_t coreset_size) {
  auto coreset = std::make_shared<KMeansCoreset>(coreset_size, set->point_size());
  coreset->Construct(set->points(), set->weights());
  return coreset;
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace pl
