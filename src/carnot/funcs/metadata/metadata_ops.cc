#include <string>

#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace funcs {
namespace metadata {

void RegisterMetadataOpsOrDie(pl::carnot::udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<ASIDUDF>("px.asid");
  registry->RegisterOrDie<UPIDToASIDUDF>("px.upid_to_asid");
  registry->RegisterOrDie<PodIDToPodNameUDF>("px.pod_id_to_pod_name");
  registry->RegisterOrDie<PodNameToPodIDUDF>("px.pod_name_to_pod_id");
  registry->RegisterOrDie<ServiceIDToServiceNameUDF>("px.service_id_to_service_name");
  registry->RegisterOrDie<ServiceNameToServiceIDUDF>("px.service_name_to_service_id");
  registry->RegisterOrDie<UPIDToContainerIDUDF>("px.upid_to_container_id");
  registry->RegisterOrDie<UPIDToNamespaceUDF>("px.upid_to_namespace");
  registry->RegisterOrDie<UPIDToPodIDUDF>("px.upid_to_pod_id");
  registry->RegisterOrDie<UPIDToPodNameUDF>("px.upid_to_pod_name");
  registry->RegisterOrDie<UPIDToServiceNameUDF>("px.upid_to_service_name");
  registry->RegisterOrDie<UPIDToServiceIDUDF>("px.upid_to_service_id");
  registry->RegisterOrDie<UPIDToNodeNameUDF>("px.upid_to_node_name");
  registry->RegisterOrDie<UPIDToHostnameUDF>("px.upid_to_hostname");
  registry->RegisterOrDie<PodIDToServiceNameUDF>("px.pod_id_to_service_name");
  registry->RegisterOrDie<PodIDToServiceIDUDF>("px.pod_id_to_service_id");
  registry->RegisterOrDie<PodNameToServiceNameUDF>("px.pod_name_to_service_name");
  registry->RegisterOrDie<PodNameToServiceIDUDF>("px.pod_name_to_service_id");
  registry->RegisterOrDie<UPIDToStringUDF>("px.upid_to_string");
  registry->RegisterOrDie<UPIDToPIDUDF>("px.upid_to_pid");
  registry->RegisterOrDie<PodIDToPodStartTimeUDF>("px.pod_id_to_start_time");
  registry->RegisterOrDie<PodNameToPodStartTimeUDF>("px.pod_name_to_start_time");
  registry->RegisterOrDie<PodNameToPodStatusUDF>("px.pod_name_to_status");
  registry->RegisterOrDie<UPIDToCmdLineUDF>("px.upid_to_cmdline");
  registry->RegisterOrDie<HostnameUDF>("px._exec_hostname");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
