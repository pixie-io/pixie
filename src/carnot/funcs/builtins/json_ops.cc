#include "src/carnot/funcs/builtins/json_ops.h"

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterJSONOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<PluckUDF>("px.pluck");
  registry->RegisterOrDie<PluckAsInt64UDF>("px.pluck_int64");
  registry->RegisterOrDie<PluckAsFloat64UDF>("px.pluck_float64");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
