#include "src/carnot/builtins/builtins.h"
#include "src/carnot/builtins/json_ops.h"
#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/builtins/math_sketches.h"
#include "src/carnot/builtins/string_ops.h"

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterBuiltinsOrDie(udf::ScalarUDFRegistry* registry) {
  RegisterMathOpsOrDie(registry);
  RegisterJSONOpsOrDie(registry);
  RegisterStringOpsOrDie(registry);
}

void RegisterBuiltinsOrDie(udf::UDARegistry* registry) {
  RegisterMathOpsOrDie(registry);
  RegisterMathSketchesOrDie(registry);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
