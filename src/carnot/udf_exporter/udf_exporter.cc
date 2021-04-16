#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/funcs/funcs.h"

namespace px {
namespace carnot {
namespace udfexporter {

StatusOr<std::unique_ptr<planner::RegistryInfo>> ExportUDFInfo() {
  auto registry = std::make_unique<udf::Registry>("udf_registry");

  vizier::funcs::VizierFuncFactoryContext ctx;
  vizier::funcs::RegisterFuncsOrDie(ctx, registry.get());

  udfspb::UDFInfo udf_proto = registry->ToProto();
  auto registry_info = std::make_unique<planner::RegistryInfo>();
  PL_RETURN_IF_ERROR(registry_info->Init(udf_proto));
  return registry_info;
}

udfspb::Docs ExportUDFDocs() {
  udf::Registry registry("udf_registry");

  vizier::funcs::VizierFuncFactoryContext ctx;
  vizier::funcs::RegisterFuncsOrDie(ctx, &registry);
  return registry.ToDocsProto();
}

}  // namespace udfexporter
}  // namespace carnot
}  // namespace px
