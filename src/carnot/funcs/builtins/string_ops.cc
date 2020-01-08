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
  registry->RegisterOrDie<ContainsUDF>("pl.contains");
  registry->RegisterOrDie<LengthUDF>("pl.length");
  registry->RegisterOrDie<FindUDF>("pl.find");
  registry->RegisterOrDie<SubstringUDF>("pl.substring");
  registry->RegisterOrDie<ToLowerUDF>("pl.tolower");
  registry->RegisterOrDie<ToUpperUDF>("pl.toupper");
  registry->RegisterOrDie<TrimUDF>("pl.trim");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
