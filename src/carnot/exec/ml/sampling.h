#pragma once

#include "src/common/base/base.h"
#include "third_party/eigen3/Eigen/Core"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

void sample_from_probs(const Eigen::ArrayXf& probs, Eigen::ArrayXi* inds, size_t sample_size);

size_t randint(size_t high);

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
