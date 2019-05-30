#include "src/carnot/builtins/string_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterStringOpsOrDie(udf::ScalarUDFRegistry* registry) {
  CHECK(registry != nullptr);
  // String contains.
  registry->RegisterOrDie<ContainsUDF>("pl.contains");
  registry->RegisterOrDie<LengthUDF>("pl.length");
  registry->RegisterOrDie<FindUDF>("pl.find");
  registry->RegisterOrDie<SubstringUDF>("pl.substring");
  registry->RegisterOrDie<ToLowerUDF>("pl.tolower");
  registry->RegisterOrDie<ToUpperUDF>("pl.toupper");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
