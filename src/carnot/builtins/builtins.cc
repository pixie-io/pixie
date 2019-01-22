#include "src/carnot/builtins/builtins.h"
#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterBuiltinsOrDie(udf::ScalarUDFRegistry* registry) { RegisterMathOpsOrDie(registry); }

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
