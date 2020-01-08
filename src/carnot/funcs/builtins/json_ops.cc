#include "src/carnot/funcs/builtins/json_ops.h"

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterJSONOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<PluckUDF>("pl.pluck");
  registry->RegisterOrDie<PluckAsInt64UDF>("pl.pluck_int64");
  registry->RegisterOrDie<PluckAsFloat64UDF>("pl.pluck_float64");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
