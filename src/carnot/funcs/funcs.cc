#include "src/carnot/funcs/funcs.h"

#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/funcs/net/net_ops.h"

namespace px {
namespace carnot {
namespace funcs {

void RegisterFuncsOrDie(udf::Registry* registry) {
  builtins::RegisterBuiltinsOrDie(registry);
  metadata::RegisterMetadataOpsOrDie(registry);
  net::RegisterNetOpsOrDie(registry);
}

}  // namespace funcs
}  // namespace carnot
}  // namespace px
