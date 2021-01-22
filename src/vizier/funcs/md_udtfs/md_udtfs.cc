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
  registry->RegisterFactoryOrDie<GetTables, UDTFWithMDFactory<GetTables>>("GetTables", ctx);
  registry->RegisterFactoryOrDie<GetTableSchemas, UDTFWithMDFactory<GetTableSchemas>>("GetSchemas",
                                                                                      ctx);
  registry->RegisterFactoryOrDie<GetAgentStatus, UDTFWithMDFactory<GetAgentStatus>>(
      "GetAgentStatus", ctx);

  registry->RegisterOrDie<GetDebugMDState>("_DebugMDState");
  registry->RegisterFactoryOrDie<GetDebugTableInfo, UDTFWithTableStoreFactory<GetDebugTableInfo>>(
      "_DebugTableInfo", ctx.table_store());

  registry->RegisterFactoryOrDie<GetUDFList, UDTFWithRegistryFactory<GetUDFList>>("GetUDFList",
                                                                                  registry);
  registry->RegisterFactoryOrDie<GetUDAList, UDTFWithRegistryFactory<GetUDAList>>("GetUDAList",
                                                                                  registry);
  registry->RegisterFactoryOrDie<GetUDTFList, UDTFWithRegistryFactory<GetUDTFList>>("GetUDTFList",
                                                                                    registry);

  registry->RegisterFactoryOrDie<GetTracepointStatus, UDTFWithMDTPFactory<GetTracepointStatus>>(
      "GetTracepointStatus", ctx);
}

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
