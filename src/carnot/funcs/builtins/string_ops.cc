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
  registry->RegisterOrDie<ContainsUDF>("contains");
  registry->RegisterOrDie<LengthUDF>("length");
  registry->RegisterOrDie<FindUDF>("find");
  registry->RegisterOrDie<SubstringUDF>("substring");
  registry->RegisterOrDie<ToLowerUDF>("tolower");
  registry->RegisterOrDie<ToUpperUDF>("toupper");
  registry->RegisterOrDie<TrimUDF>("trim");
  registry->RegisterOrDie<StripPrefixUDF>("strip_prefix");
  registry->RegisterOrDie<HexToASCII>("hex_to_ascii");
  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
