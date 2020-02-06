#include "src/carnot/funcs/builtins/json_ops.h"

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterJSONOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<PluckUDF>("pluck");
  registry->RegisterOrDie<PluckAsInt64UDF>("pluck_int64");
  registry->RegisterOrDie<PluckAsFloat64UDF>("pluck_float64");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
