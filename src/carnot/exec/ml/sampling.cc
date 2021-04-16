#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

#include "src/common/base/base.h"
#include "third_party/eigen3/Eigen/Core"

#include "src/carnot/exec/ml/sampling.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

void sample_from_probs(const Eigen::ArrayXf& probs, Eigen::ArrayXi* inds, size_t sample_size) {
  DCHECK_LT(pow(1.0f - probs.sum(), 2), 1e-4);
  inds->resize(sample_size);
  // cumsum for probs
  std::vector<float> cumsum(probs.size());
  std::partial_sum(probs.begin(), probs.end(), cumsum.begin());

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  for (auto i = 0UL; i < sample_size; i++) {
    auto it = std::lower_bound(cumsum.begin(), cumsum.end(), dis(gen));
    inds->operator()(i) = it - cumsum.begin();
  }
}

size_t randint(size_t high) { return rand() % high; }

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
