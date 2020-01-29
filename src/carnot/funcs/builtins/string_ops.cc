#include "src/carnot/funcs/builtins/string_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterStringOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  // String contains.
  registry->RegisterOrDie<ContainsUDF>("px.contains");
  registry->RegisterOrDie<LengthUDF>("px.length");
  registry->RegisterOrDie<FindUDF>("px.find");
  registry->RegisterOrDie<SubstringUDF>("px.substring");
  registry->RegisterOrDie<ToLowerUDF>("px.tolower");
  registry->RegisterOrDie<ToUpperUDF>("px.toupper");
  registry->RegisterOrDie<TrimUDF>("px.trim");
  registry->RegisterOrDie<StripPrefixUDF>("px.strip_prefix");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
