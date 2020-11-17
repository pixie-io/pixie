#include "src/carnot/funcs/builtins/json_ops.h"

#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

using types::StringValue;

void RegisterJSONOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<PluckUDF>("pluck");
  registry->RegisterOrDie<PluckAsInt64UDF>("pluck_int64");
  registry->RegisterOrDie<PluckAsFloat64UDF>("pluck_float64");

  // 0 script args
  registry->RegisterOrDie<ScriptReferenceUDF<>>("_script_reference");

  // 1 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue>>("_script_reference");
  // 2 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
  // 3 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue>>("_script_reference");
  // 4 script args
  registry->RegisterOrDie<ScriptReferenceUDF<StringValue, StringValue, StringValue, StringValue,
                                             StringValue, StringValue, StringValue, StringValue>>(
      "_script_reference");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
