#include "src/vizier/funcs/funcs.h"
#include "src/carnot/funcs/funcs.h"
#include "src/vizier/funcs/md_udtfs/md_udtfs.h"
namespace pl {
namespace vizier {
namespace funcs {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry) {
  // All used functions must be registered here.
  ::pl::carnot::funcs::RegisterFuncsOrDie(registry);
  md::RegisterFuncsOrDie(ctx, registry);
}

}  // namespace funcs
}  // namespace vizier
}  // namespace pl
