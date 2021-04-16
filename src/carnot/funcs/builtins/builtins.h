#pragma once

#include "src/carnot/udf/registry.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterBuiltinsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
