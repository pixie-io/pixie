#include "src/vizier/funcs/funcs.h"
#include "src/carnot/funcs/funcs.h"
#include "src/vizier/funcs/internal/internal_impl.h"
#include "src/vizier/funcs/md_udtfs/md_udtfs.h"
namespace px {
namespace vizier {
namespace funcs {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry) {
  // All used functions must be registered here.
  ::px::carnot::funcs::RegisterFuncsOrDie(registry);
  md::RegisterFuncsOrDie(ctx, registry);
  internal::RegisterFuncsOrDie(registry);
}

}  // namespace funcs
}  // namespace vizier
}  // namespace px
