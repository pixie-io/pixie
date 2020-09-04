#include <string>

#include "src/carnot/funcs/net/net_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace funcs {
namespace net {

void RegisterNetOpsOrDie(pl::carnot::udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<NSLookupUDF>("nslookup");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace net
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
