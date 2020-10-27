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
  registry->RegisterOrDie<ASIDUDF>("asid");
  registry->RegisterOrDie<ContainerIDToContainerStatusUDF>("container_id_to_status");
  registry->RegisterOrDie<ContainerIDToContainerStartTimeUDF>("container_id_to_start_time");
  registry->RegisterOrDie<ContainerIDToContainerStopTimeUDF>("container_id_to_stop_time");
  registry->RegisterOrDie<ContainerNameToContainerIDUDF>("container_name_to_container_id");
  registry->RegisterOrDie<ContainerNameToContainerStartTimeUDF>("container_name_to_start_time");
  registry->RegisterOrDie<ContainerNameToContainerStopTimeUDF>("container_name_to_stop_time");
  registry->RegisterOrDie<PodIPToPodIDUDF>("ip_to_pod_id");
  registry->RegisterOrDie<PodIDToPodNameUDF>("pod_id_to_pod_name");
  registry->RegisterOrDie<PodIDToNamespaceUDF>("pod_id_to_namespace");
  registry->RegisterOrDie<PodIDToNodeNameUDF>("pod_id_to_node_name");
  registry->RegisterOrDie<PodIDToPodStartTimeUDF>("pod_id_to_start_time");
  registry->RegisterOrDie<PodIDToPodStopTimeUDF>("pod_id_to_stop_time");
  registry->RegisterOrDie<PodIDToServiceNameUDF>("pod_id_to_service_name");
  registry->RegisterOrDie<PodIDToServiceIDUDF>("pod_id_to_service_id");
  registry->RegisterOrDie<PodNameToNamespaceUDF>("pod_name_to_namespace");
  registry->RegisterOrDie<PodNameToPodIDUDF>("pod_name_to_pod_id");
  registry->RegisterOrDie<PodNameToServiceNameUDF>("pod_name_to_service_name");
  registry->RegisterOrDie<PodNameToServiceIDUDF>("pod_name_to_service_id");
  registry->RegisterOrDie<PodNameToPodStartTimeUDF>("pod_name_to_start_time");
  registry->RegisterOrDie<PodNameToPodStartTimeUDF>("pod_name_to_stop_time");
  registry->RegisterOrDie<PodNameToPodStatusUDF>("pod_name_to_status");
  registry->RegisterOrDie<ServiceIDToServiceNameUDF>("service_id_to_service_name");
  registry->RegisterOrDie<ServiceNameToServiceIDUDF>("service_name_to_service_id");
  registry->RegisterOrDie<ServiceNameToNamespaceUDF>("service_name_to_namespace");
  registry->RegisterOrDie<UPIDToASIDUDF>("upid_to_asid");
  registry->RegisterOrDie<UPIDToContainerIDUDF>("upid_to_container_id");
  registry->RegisterOrDie<UPIDToCmdLineUDF>("upid_to_cmdline");
  registry->RegisterOrDie<UPIDToContainerNameUDF>("upid_to_container_name");
  registry->RegisterOrDie<UPIDToHostnameUDF>("upid_to_hostname");
  registry->RegisterOrDie<UPIDToNamespaceUDF>("upid_to_namespace");
  registry->RegisterOrDie<UPIDToNodeNameUDF>("upid_to_node_name");
  registry->RegisterOrDie<UPIDToPIDUDF>("upid_to_pid");
  registry->RegisterOrDie<UPIDToPodIDUDF>("upid_to_pod_id");
  registry->RegisterOrDie<UPIDToPodNameUDF>("upid_to_pod_name");
  registry->RegisterOrDie<UPIDToPodQoSUDF>("upid_to_pod_qos");
  registry->RegisterOrDie<UPIDToPodStatusUDF>("upid_to_pod_status");
  registry->RegisterOrDie<UPIDToServiceNameUDF>("upid_to_service_name");
  registry->RegisterOrDie<UPIDToServiceIDUDF>("upid_to_service_id");
  registry->RegisterOrDie<UPIDToStringUDF>("upid_to_string");
  registry->RegisterOrDie<HostnameUDF>("_exec_hostname");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
