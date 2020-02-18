#pragma once

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"

namespace pl {
namespace vizier {
namespace funcs {
namespace internal {

void RegisterFuncsOrDie(carnot::udf::Registry* registry);

}  // namespace internal
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
