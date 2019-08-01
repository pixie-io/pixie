#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/funcs/builtins/json_ops.h"
#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/funcs/builtins/math_sketches.h"
#include "src/carnot/funcs/builtins/string_ops.h"

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
