#include <string>

#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace funcs {
namespace metadata {

void RegisterMetadataOpsOrDie(pl::carnot::udf::ScalarUDFRegistry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<ASIDUDF>("pl.asid");
  registry->RegisterOrDie<PodIDToPodNameUDF>("pl.pod_id_to_pod_name");
  registry->RegisterOrDie<PodNameToPodIDUDF>("pl.pod_name_to_pod_id");
  registry->RegisterOrDie<UPIDToContainerIDUDF>("pl.upid_to_container_id");
  registry->RegisterOrDie<UPIDToPodIDUDF>("pl.upid_to_pod_id");
  registry->RegisterOrDie<UPIDToPodNameUDF>("pl.upid_to_pod_name");
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
