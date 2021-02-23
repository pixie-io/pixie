#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/funcs/builtins/collections.h"
#include "src/carnot/funcs/builtins/json_ops.h"
#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/funcs/builtins/math_sketches.h"
#include "src/carnot/funcs/builtins/ml_ops.h"
#include "src/carnot/funcs/builtins/string_ops.h"

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterBuiltinsOrDie(udf::Registry* registry) {
  RegisterCollectionOpsOrDie(registry);
  RegisterMathOpsOrDie(registry);
  RegisterMathSketchesOrDie(registry);
  RegisterJSONOpsOrDie(registry);
  RegisterStringOpsOrDie(registry);
  RegisterMLOpsOrDie(registry);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
