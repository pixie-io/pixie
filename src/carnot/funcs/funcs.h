#pragma once

#include "src/carnot/udf/registry.h"

namespace px {
namespace carnot {
namespace funcs {

void RegisterFuncsOrDie(udf::Registry* registry);

}  // namespace funcs
}  // namespace carnot
}  // namespace px
