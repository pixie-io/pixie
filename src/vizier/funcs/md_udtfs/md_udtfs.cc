#include "src/vizier/funcs/md_udtfs/md_udtfs.h"

#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/funcs/md_udtfs/md_udtfs_impl.h"

namespace pl {
namespace vizier {
namespace funcs {
namespace md {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry) {
  registry->RegisterFactoryOrDie<GetTableSchemas, GetTableSchemas::Factory>("GetSchemas", ctx);
}

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
