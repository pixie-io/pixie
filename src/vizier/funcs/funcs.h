#pragma once

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace vizier {
namespace funcs {

void RegisterFuncsOrDie(carnot::udf::Registry* registry);

}  // namespace funcs
}  // namespace vizier
}  // namespace pl
