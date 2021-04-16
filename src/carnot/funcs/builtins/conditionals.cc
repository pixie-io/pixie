#include "src/carnot/funcs/builtins/conditionals.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterConditionalOpsOrDie(udf::Registry* registry) {
  // Select.
  registry->RegisterOrDie<SelectUDF<types::BoolValue>>("select");
  registry->RegisterOrDie<SelectUDF<types::Int64Value>>("select");
  registry->RegisterOrDie<SelectUDF<types::Float64Value>>("select");
  registry->RegisterOrDie<SelectUDF<types::Time64NSValue>>("select");
  registry->RegisterOrDie<SelectUDF<types::StringValue>>("select");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
