#pragma once

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterBuiltinsOrDie(udf::ScalarUDFRegistry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
