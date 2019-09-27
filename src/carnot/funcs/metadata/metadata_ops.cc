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
  registry->RegisterOrDie<ServiceIDToServiceNameUDF>("pl.service_id_to_service_name");
  registry->RegisterOrDie<ServiceNameToServiceIDUDF>("pl.service_name_to_service_id");
  registry->RegisterOrDie<UPIDToContainerIDUDF>("pl.upid_to_container_id");
  registry->RegisterOrDie<UPIDToPodIDUDF>("pl.upid_to_pod_id");
  registry->RegisterOrDie<UPIDToPodNameUDF>("pl.upid_to_pod_name");
  registry->RegisterOrDie<UPIDToServiceNameUDF>("pl.upid_to_service_name");
  registry->RegisterOrDie<UPIDToPodIDUDF>("pl.upid_to_service_id");
  registry->RegisterOrDie<PodIDToServiceNameUDF>("pl.pod_id_to_service_name");
  registry->RegisterOrDie<PodIDToServiceIDUDF>("pl.pod_id_to_service_id");
  registry->RegisterOrDie<PodNameToServiceNameUDF>("pl.pod_name_to_service_name");
  registry->RegisterOrDie<PodNameToServiceIDUDF>("pl.pod_name_to_service_id");
  registry->RegisterOrDie<UPIDToStringUDF>("pl.upid_to_string");
  registry->RegisterOrDie<UPIDToPIDUDF>("pl.upid_to_pid");
  registry->RegisterOrDie<PodIDToPodStartTimeUDF>("pl.pod_id_to_start_time");
  registry->RegisterOrDie<PodNameToPodStartTimeUDF>("pl.pod_name_to_start_time");
  registry->RegisterOrDie<PodNameToPodStatusUDF>("pl.pod_name_to_status");
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
