#include "src/carnot/funcs/builtins/collections.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterCollectionOpsOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<AnyUDA<types::BoolValue>>("any");
  registry->RegisterOrDie<AnyUDA<types::Int64Value>>("any");
  registry->RegisterOrDie<AnyUDA<types::Float64Value>>("any");
  registry->RegisterOrDie<AnyUDA<types::Time64NSValue>>("any");
  registry->RegisterOrDie<AnyUDA<types::StringValue>>("any");
  registry->RegisterOrDie<AnyUDA<types::UInt128Value>>("any");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
