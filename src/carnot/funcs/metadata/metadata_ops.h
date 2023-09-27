/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/sysinfo.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/funcs/shared/utils.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
namespace metadata {

using ScalarUDF = px::carnot::udf::ScalarUDF;
using K8sNameIdentView = px::md::K8sMetadataState::K8sNameIdentView;

namespace internal {
inline rapidjson::GenericStringRef<char> StringRef(std::string_view s) {
  return rapidjson::GenericStringRef<char>(s.data(), s.size());
}

inline StatusOr<K8sNameIdentView> K8sName(std::string_view name) {
  std::vector<std::string_view> name_parts = absl::StrSplit(name, "/");
  if (name_parts.size() != 2) {
    return error::Internal("Malformed K8s name: $0", name);
  }
  return std::make_pair(name_parts[0], name_parts[1]);
}

}  // namespace internal

inline const px::md::AgentMetadataState* GetMetadataState(px::carnot::udf::FunctionContext* ctx) {
  DCHECK(ctx != nullptr);
  auto md = ctx->metadata_state();
  DCHECK(md != nullptr);
  return md;
}

class ASIDUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);
    return md->asid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the agent ID.")
        .Details("Get the agent ID of the node that the data originated from.")
        .Example("df.agent = px.asid()")
        .Returns("The agent ID.");
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<ASIDUDF>(types::ST_ASID, {})};
  }
};

class UPIDToASIDUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, UInt128Value upid_value) { return upid_value.High64() >> 32; }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Pixie Agent ID from the UPID.")
        .Details(
            "Gets the Pixie Agent ID from the given Unique Process ID (UPID). "
            "The Pixie Agent ID signifies which Pixie Agent is tracing the given process.")
        .Example("df.agent_id = px.upid_to_asid(df.upid)")
        .Arg("upid", "The UPID of the process to get the Pixie Agent ID for.")
        .Returns("The Pixie Agent ID for the UPID passed in.");
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<UPIDToASIDUDF>(types::ST_ASID, {types::ST_NONE})};
  }
};

class PodIDToPodNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info != nullptr) {
      return absl::Substitute("$0/$1", pod_info->ns(), pod_info->name());
    }

    return "";
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<PodIDToPodNameUDF>(types::ST_POD_NAME, {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the name of a pod from its pod ID.")
        .Details("Gets the kubernetes name for the pod from its pod ID.")
        .Example("df.pod_name = px.pod_id_to_pod_name(df.pod_id)")
        .Arg("pod_id", "The pod ID of the pod to get the name for.")
        .Returns("The k8s pod name for the pod ID passed in.");
  }
};

class PodIDToPodLabelsUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info != nullptr) {
      return pod_info->labels();
    }
    return "";
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get labels of a pod from its pod ID.")
        .Details("Gets the kubernetes pod labels for the pod from its pod ID.")
        .Example("df.labels = px.pod_id_to_pod_labels(df.pod_id)")
        .Arg("pod_id", "The pod ID of the pod to get the labels for.")
        .Returns("The k8s pod labels for the pod ID passed in.");
  }
};

class PodNameToPodIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    return GetPodID(md, pod_name);
  }

  static StringValue GetPodID(const px::md::AgentMetadataState* md, StringValue pod_name) {
    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);
    return pod_id;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the id of a pod from its name.")
        .Details("Gets the kubernetes ID for the pod from its name.")
        .Example("df.pod_id = px.pod_name_to_pod_id(df.pod_name)")
        .Arg("pod_name", "The name of the pod to get the ID for.")
        .Returns("The k8s pod ID for the pod name passed in.");
  }
};

class PodNameToPodIPUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    const px::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }
    return pod_info->pod_ip();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the IP address of a pod from its name.")
        .Details("Gets the IP address for the pod from its name.")
        .Example("df.pod_ip = px.pod_name_to_pod_ip(df.pod_name)")
        .Arg("pod_name", "The name of the pod to get the IP for.")
        .Returns("The pod IP for the pod name passed in.");
  }
};

class PodIDToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info != nullptr) {
      return pod_info->ns();
    }

    return "";
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::ExplicitRule::Create<PodIDToNamespaceUDF>(types::ST_NAMESPACE_NAME, {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes namespace from a pod ID.")
        .Details("Gets the Kubernetes namespace that the Pod ID belongs to.")
        .Example("df.namespace = px.pod_id_to_namespace(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the namespace for.")
        .Returns("The k8s namespace for the Pod ID passed in.");
  }
};

class PodNameToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue pod_name) {
    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto k8s_name_view, internal::K8sName(pod_name), return "");
    return std::string(k8s_name_view.first);
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<PodNameToNamespaceUDF>(types::ST_NAMESPACE_NAME,
                                                             {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes namespace from a pod name.")
        .Details("Gets the Kubernetes namespace that the pod belongs to.")
        .Example("df.namespace = px.pod_name_to_namespace(df.pod_name)")
        .Arg("pod_name", "The name of the Pod to get the namespace for.")
        .Returns("The k8s namespace for the pod passed in.");
  }
};

class UPIDToContainerIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);

    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    auto pid = md->GetPIDByUPID(upid);
    if (pid == nullptr) {
      return "";
    }
    return pid->cid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes container ID from a UPID.")
        .Details(
            "Gets the Kubernetes container ID for the container the process "
            "with the given Unique Process ID (UPID) is running on. "
            "If the UPID has no associated Kubernetes container, this function will return an "
            "empty string")
        .Example("df.container_id = px.upid_to_container_id(df.upid)")
        .Arg("upid", "The UPID of the process to get the container ID for.")
        .Returns("The k8s container ID for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

inline const md::ContainerInfo* UPIDToContainer(const px::md::AgentMetadataState* md,
                                                types::UInt128Value upid_value) {
  auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
  auto upid = md::UPID(upid_uint128);
  auto pid = md->GetPIDByUPID(upid);
  if (pid == nullptr) {
    return nullptr;
  }
  return md->k8s_metadata_state().ContainerInfoByID(pid->cid());
}

class UPIDToContainerNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto container_info = UPIDToContainer(md, upid_value);
    if (container_info == nullptr) {
      return "";
    }
    return std::string(container_info->name());
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<UPIDToContainerNameUDF>(types::ST_CONTAINER_NAME,
                                                              {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes container name from a UPID.")
        .Details(
            "Gets the Kubernetes container name for the container the process "
            "with the given Unique Process ID (UPID) is running on. "
            "If the UPID has no associated Kubernetes container, this function will return an "
            "empty string")
        .Example("df.container_name = px.upid_to_container_name(df.upid)")
        .Arg("upid", "The UPID of the process to get the container name for.")
        .Returns("The k8s container name for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

inline const px::md::PodInfo* UPIDtoPod(const px::md::AgentMetadataState* md,
                                        types::UInt128Value upid_value) {
  auto container_info = UPIDToContainer(md, upid_value);
  if (container_info == nullptr) {
    return nullptr;
  }
  auto pod_info = md->k8s_metadata_state().PodInfoByID(container_info->pod_id());
  return pod_info;
}

class UPIDToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }
    return pod_info->ns();
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::ExplicitRule::Create<UPIDToNamespaceUDF>(types::ST_NAMESPACE_NAME, {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes namespace from a UPID.")
        .Details(
            "Gets the Kubernetes namespace for the process "
            "with the given Unique Process ID (UPID). "
            "If the process is not running within a kubernetes context, this function will return "
            "an empty string")
        .Example("df.namespace = px.upid_to_namespace(df.upid)")
        .Arg("upid", "The UPID of the process to get the namespace for.")
        .Returns("The k8s namespace for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

class UPIDToPodIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto container_info = UPIDToContainer(md, upid_value);
    if (container_info == nullptr) {
      return "";
    }
    return std::string(container_info->pod_id());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes Pod ID from a UPID.")
        .Details(
            "Gets the Kubernetes pod ID for the pod the process "
            "with the given Unique Process ID (UPID) is running on. "
            "If the UPID has no associated Kubernetes Pod, this function will return an empty "
            "string.")
        .Example("df.pod_id = px.upid_to_pod_id(df.upid)")
        .Arg("upid", "The UPID of the process to get the pod ID for.")
        .Returns("The k8s pod ID for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

class UPIDToPodNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }
    return absl::Substitute("$0/$1", pod_info->ns(), pod_info->name());
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<UPIDToPodNameUDF>(types::ST_POD_NAME, {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes Pod Name from a UPID.")
        .Details(
            "Gets the name of Kubernetes pod the process "
            "with the given Unique Process ID (UPID) is running on. "
            "If the UPID has no associated Kubernetes Pod, this function will return an empty "
            "string")
        .Example("df.pod_name = px.upid_to_pod_name(df.upid)")
        .Arg("upid", "The UPID of the process to get the pod name for.")
        .Returns("The k8s pod name for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

class ServiceIDToServiceNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue service_id) {
    auto md = GetMetadataState(ctx);

    const auto* service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
    if (service_info != nullptr) {
      return absl::Substitute("$0/$1", service_info->ns(), service_info->name());
    }

    return "";
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<ServiceIDToServiceNameUDF>(types::ST_SERVICE_NAME,
                                                                 {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert the Kubernetes service ID to service name.")
        .Details(
            "Converts the Kubernetes service ID to the name of the service. If the ID "
            "is not found in our mapping, then returns an empty string.")
        .Example("df.service = px.service_id_to_service_name(df.service_id)")
        .Arg("service_id", "The service ID to get the service name for.")
        .Returns("The service name or an empty string if service_id not found.");
  }
};

class ServiceIDToClusterIPUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue service_id) {
    auto md = GetMetadataState(ctx);
    const auto* service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
    if (service_info != nullptr) {
      return service_info->cluster_ip();
    }
    return "";
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::ExplicitRule::Create<ServiceIDToClusterIPUDF>(types::ST_IP_ADDRESS, {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert the Kubernetes service ID to its cluster IP.")
        .Details(
            "Converts the Kubernetes service ID to the cluster IP of the service. If either "
            "the service ID or IP is not found in our mapping, then returns an empty string.")
        .Example("df.cluster_ip = px.service_id_to_cluster_ip(df.service_id)")
        .Arg("service_id", "The service ID to get the service name for.")
        .Returns("The cluster IP or an empty string.");
  }
};

class ServiceIDToExternalIPsUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue service_id) {
    auto md = GetMetadataState(ctx);
    const auto* service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
    if (service_info != nullptr) {
      return VectorToStringArray(service_info->external_ips());
    }
    return "";
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Convert the Kubernetes service ID to its external IP addresses.")
        .Details(
            "Converts the Kubernetes service ID to the external IPs of the service as an array. "
            "If the the service ID is not found in our mapping, then returns an empty string. "
            "If the service ID is found but has no external IPs, then returns an empty array. ")
        .Example("df.external_ips = px.service_id_to_external_ips(df.service_id)")
        .Arg("service_id", "The service ID to get the service name for.")
        .Returns("The external IPs or an empty string.");
  }
};

class ServiceNameToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue service_name) {
    auto md = GetMetadataState(ctx);
    // This UDF expects the service name to be in the format of "<ns>/<service-name>".
    PX_ASSIGN_OR(auto service_name_view, internal::K8sName(service_name), return "");
    auto service_id = md->k8s_metadata_state().ServiceIDByName(service_name_view);
    return service_id;
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert the service name to the service ID.")
        .Details(
            "Converts the service name to the corresponding kubernetes service ID. If the name "
            "is not found in our mapping, the function returns an empty string.")
        .Example("df.service_id = px.service_name_to_service_id(df.service)")
        .Arg("service_name", "The service to get the service ID.")
        .Returns("The kubernetes service ID for the service passed in.");
  }
};

/**
 * @brief Returns the namespace for a service.
 */
class ServiceNameToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue service_name) {
    // This UDF expects the service name to be in the format of "<ns>/<svc-name>".
    PX_ASSIGN_OR(auto service_name_view, internal::K8sName(service_name), return "");
    return std::string(service_name_view.first);
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<ServiceNameToNamespaceUDF>(types::ST_NAMESPACE_NAME,
                                                                 {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Gets the namespace from the service name.")
        .Details(
            "Extracts the namespace from the service name. It expects the service name to come in "
            "the format"
            "`<namespace>/<service_name>`, otherwise it'll return an empty string.")
        .Example(R"doc(# df.service is `pl/kelvin`
        | df.namespace = px.service_name_to_namespace(df.service) # "pl"
        )doc")
        .Arg("service_name", "The service to extract the namespace.")
        .Returns("The namespace of the service.");
  }
};

/**
 * @brief Returns the service ids for services that are currently running.
 */
class UPIDToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr || pod_info->services().size() == 0) {
      return "";
    }
    std::vector<std::string> running_service_ids;
    for (const auto& service_id : pod_info->services()) {
      auto service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
      if (service_info == nullptr) {
        continue;
      }
      if (service_info->stop_time_ns() == 0) {
        running_service_ids.push_back(service_id);
      }
    }

    return StringifyVector(running_service_ids);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Service ID from a UPID.")
        .Details(
            "Gets the Kubernetes Service ID for the process with the given Unique Process ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes service, this function "
            "returns "
            "an empty string.")
        .Example("df.service_id = px.upid_to_service_id(df.upid)")
        .Arg("upid", "The UPID of the process to get the service ID for.")
        .Returns("The kubernetes service ID for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the service names for services that are currently running.
 */
class UPIDToServiceNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr || pod_info->services().size() == 0) {
      return "";
    }
    std::vector<std::string> running_service_names;
    for (const auto& service_id : pod_info->services()) {
      auto service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
      if (service_info == nullptr) {
        continue;
      }
      if (service_info->stop_time_ns() == 0) {
        running_service_names.push_back(
            absl::Substitute("$0/$1", service_info->ns(), service_info->name()));
      }
    }
    return StringifyVector(running_service_names);
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::ExplicitRule::Create<UPIDToServiceNameUDF>(types::ST_SERVICE_NAME, {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Service Name from a UPID.")
        .Details(
            "Gets the Kubernetes Service Name for the process with the given Unique Process ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes service, this function "
            "returns "
            "an empty string")
        .Example("df.service_name = px.upid_to_service_name(df.upid)")
        .Arg("upid", "The UPID of the process to get the service name for.")
        .Returns("The Kubernetes Service Name for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the node name for the pod associated with the input upid.
 */
class UPIDToNodeNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }
    std::string foo = std::string(pod_info->node_name());
    return foo;
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<UPIDToNodeNameUDF>(types::ST_NODE_NAME, {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Node Name from a UPID.")
        .Details(
            "Gets the Kubernetes name of the node the process "
            "with the given Unique Process ID (UPID) is running on.")
        .Example("df.node_name = px.upid_to_node_name(df.upid)")
        .Arg("upid", "The UPID of the process to get the node name for.")
        .Returns("The name of the node for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns string representation of condition status
 */
inline std::string conditionStatusToString(md::ConditionStatus status) {
  switch (status) {
    case md::ConditionStatus::kTrue:
      return "True";
    case md::ConditionStatus::kFalse:
      return "False";
    default:
      return "Unknown";
  }
}

/**
 * @brief Converts Replica Set info to a json string.
 */

inline types::StringValue ReplicaSetInfoToStatus(const px::md::ReplicaSetInfo* rs_info) {
  int replicas = 0;
  int fully_labeled_replicas = 0;
  int ready_replicas = 0;
  int available_replicas = 0;
  int observed_generation = 0;
  int requested_replicas = 0;
  std::string conditions = "";

  if (rs_info != nullptr) {
    replicas = rs_info->replicas();
    fully_labeled_replicas = rs_info->fully_labeled_replicas();
    ready_replicas = rs_info->ready_replicas();
    available_replicas = rs_info->available_replicas();
    observed_generation = rs_info->observed_generation();
    requested_replicas = rs_info->requested_replicas();

    auto rs_conditions = rs_info->conditions();
    for (const auto& condition : rs_info->conditions()) {
      std::string conditionStatus;
      conditions = absl::Substitute(R"("$0": "$1", $2)", condition.first,
                                    conditionStatusToString(condition.second), conditions);
    }
  }

  rapidjson::Document d;
  d.SetObject();
  d.AddMember("replicas", replicas, d.GetAllocator());
  d.AddMember("fully_labeled_replicas", fully_labeled_replicas, d.GetAllocator());
  d.AddMember("ready_replicas", ready_replicas, d.GetAllocator());
  d.AddMember("available_replicas", available_replicas, d.GetAllocator());
  d.AddMember("requested_replicas", requested_replicas, d.GetAllocator());
  d.AddMember("observed_generation", observed_generation, d.GetAllocator());
  d.AddMember("conditions", internal::StringRef(conditions), d.GetAllocator());
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  d.Accept(writer);
  return sb.GetString();
}

// Converts owner reference object into a json string
inline types::StringValue OwnerReferenceString(const px::md::OwnerReference& owner_reference) {
  std::string uid = owner_reference.uid;
  std::string kind = owner_reference.kind;
  std::string name = owner_reference.name;

  rapidjson::Document d;
  d.SetObject();
  d.AddMember("uid", internal::StringRef(uid), d.GetAllocator());
  d.AddMember("kind", internal::StringRef(kind), d.GetAllocator());
  d.AddMember("name", internal::StringRef(name), d.GetAllocator());
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  d.Accept(writer);
  return sb.GetString();
}

/**
 * @brief Returns the Replica Set name for the given Replica Set ID.
 * @brief Returns string representation of Deployment condition status
 */
inline std::string ConditionTypeToString(md::DeploymentConditionType status) {
  switch (status) {
    case md::DeploymentConditionType::kAvailable:
      return "Available";
    case md::DeploymentConditionType::kProgressing:
      return "Progressing";
    case md::DeploymentConditionType::kReplicaFailure:
      return "ReplicaFailure";
    default:
      return "Unknown";
  }
}

/**
 * @brief Converts Deployment info to a json string.
 */

inline types::StringValue DeploymentInfoToStatus(const px::md::DeploymentInfo* dep_info) {
  int replicas = 0;
  int updated_replicas = 0;
  int ready_replicas = 0;
  int available_replicas = 0;
  int unavailable_replicas = 0;
  int observed_generation = 0;
  int requested_replicas = 0;

  std::string conditions = "";

  if (dep_info != nullptr) {
    replicas = dep_info->replicas();
    updated_replicas = dep_info->updated_replicas();
    ready_replicas = dep_info->ready_replicas();
    available_replicas = dep_info->available_replicas();
    unavailable_replicas = dep_info->unavailable_replicas();
    observed_generation = dep_info->observed_generation();
    requested_replicas = dep_info->requested_replicas();

    auto rs_conditions = dep_info->conditions();
    for (const auto& condition : dep_info->conditions()) {
      std::string conditionStatus;
      conditions = absl::Substitute(R"("$0": "$1", $2)", ConditionTypeToString(condition.first),
                                    conditionStatusToString(condition.second), conditions);
    }
  }

  rapidjson::Document d;
  d.SetObject();
  d.AddMember("replicas", replicas, d.GetAllocator());
  d.AddMember("updated_replicas", updated_replicas, d.GetAllocator());
  d.AddMember("ready_replicas", ready_replicas, d.GetAllocator());
  d.AddMember("available_replicas", available_replicas, d.GetAllocator());
  d.AddMember("unavailable_replicas", unavailable_replicas, d.GetAllocator());
  d.AddMember("requested_replicas", requested_replicas, d.GetAllocator());
  d.AddMember("observed_generation", observed_generation, d.GetAllocator());
  d.AddMember("conditions", internal::StringRef(conditions), d.GetAllocator());
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  d.Accept(writer);
  return sb.GetString();
}

/**
 * @brief Returns the replica set id for the given replica set name.
 */
class ReplicaSetIDToReplicaSetNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", rs_info->ns(), rs_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Replica Set Name from a Replica Set ID.")
        .Details(
            "Gets the Kubernetes Replica Set Name for the Replica Set ID."
            "If the given ID doesn't have an associated Kubernetes Replica Set, this function "
            "returns "
            "an empty string")
        .Example("df.replica_set_name = px.replicaset_id_to_replicaset_name(replica_set_id)")
        .Arg("replica_set_id", "The UID of the Replica Set to get the name for.")
        .Returns("The Kubernetes Replica Set Name for the UID passed in.");
  }
};

/**
 * @brief Returns the Replica Set start time for Replica Set ID.
 */
class ReplicaSetIDToStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return 0;
    }
    return rs_info->start_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a Replica Set from its ID.")
        .Details(
            "Gets the start time (in nanosecond unix time format) of a Replica Set from its ID.")
        .Example("df.rs_start_time = px.replicaset_id_to_start_time(replica_set_id)")
        .Arg("replica_set_id", "The Replica Set ID of the Replica Set to get the start time for.")
        .Returns("The start time (as an integer) for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Replica Set stop time for Replica Set ID.
 */
class ReplicaSetIDToStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return 0;
    }
    return rs_info->stop_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a Replica Set from its ID.")
        .Details(
            "Gets the stop time (in nanosecond unix time format) of a Replica Set from its ID.")
        .Example("df.rs_stop_time = px.replicaset_id_to_stop_time(replica_set_id)")
        .Arg("replica_set_id", "The Replica Set ID of the Replica Set to get the stop time for.")
        .Returns("The stop time (as an integer) for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Replica Set namespace for Replica Set ID.
 */
class ReplicaSetIDToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }
    return rs_info->ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the namespace of a Replica Set from its ID.")
        .Details("Gets the namespace of a Replica Set from its ID.")
        .Example("df.namespace = px.replicaset_id_to_namespace(replica_set_id)")
        .Arg("replica_set_id", "The Replica Set ID of the Replica Set to get the namespace for.")
        .Returns("The namespace for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Replica Set owner references for Replica Sets ID.
 */
class ReplicaSetIDToOwnerReferencesUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    std::vector<std::string> owner_references;
    for (const auto& owner_reference : rs_info->owner_references()) {
      owner_references.push_back(OwnerReferenceString(owner_reference));
    }

    return VectorToStringArray(owner_references);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the owner references of a Replica Set from its ID.")
        .Details("Gets the owner references of a Replica Set from its ID.")
        .Example("df.owner_references = px.replicaset_id_to_owner_references(replica_set_id)")
        .Arg("replica_set_id",
             "The Replica Set ID of the Replica Set to get the owner references for.")
        .Returns("The owner references for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Replica Set status for Replica Set ID.
 */
class ReplicaSetIDToStatusUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    return ReplicaSetInfoToStatus(rs_info);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the status of a Replica Set from its ID.")
        .Details("Gets the status of a Replica Set from its ID.")
        .Example("df.status = px.replicaset_id_to_status(replica_set_id)")
        .Arg("replica_set_id", "The Replica Set ID of the Replica Set to get the status for.")
        .Returns("The status for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Deployment name for Replica Set ID.
 */
class ReplicaSetIDToDeploymentNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", dep_info->ns(), dep_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment name of a Replica Set from its ID.")
        .Details("Gets the Deployment name of a Replica Set from its ID.")
        .Example("df.deployment_name =px.replicaset_id_to_deployment_name(replica_set_id)")
        .Arg("replica_set_id",
             "The Replica Set ID of the Replica Set to get the Deployment name for.")
        .Returns("The Deployment name for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Deployment ID for Replica Set ID.
 */
class ReplicaSetIDToDeploymentIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_id) {
    auto md = GetMetadataState(ctx);
    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return dep_info->uid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment ID of a Replica Set from its ID.")
        .Details("Gets the Deployment ID of a Replica Set from its ID.")
        .Example("df.deployment_id =px.replicaset_id_to_deployment_id(replica_set_id)")
        .Arg("replica_set_id",
             "The Replica Set ID of the Replica Set to get the Deployment ID for.")
        .Returns("The Deployment ID for the Replica Set ID passed in.");
  }
};

/**
 * @brief Returns the Replica Set name from Replica Set ID.
 */
class ReplicaSetNameToReplicaSetIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return "");
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    return replica_set_id;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Replica Set ID from a Replica Set name.")
        .Details(
            "Gets the Kubernetes Replica Set ID for the Replica Set name."
            "If the given name doesn't have an associated Kubernetes Replica Set, this function "
            "returns "
            "an empty string")
        .Example("df.replica_set_id = px.replicaset_name_to_replicaset_id(replica_set_name)")
        .Arg("replica_set_name",
             "The name of the Replica Set to get the name for. The name includes the namespace of "
             "the Replica Set. i.e. \"ns/rs_name\"")
        .Returns("The Kubernetes Replica Set ID for the name passed in.");
  }
};

/**
 * @brief Returns the Replica Set start time for Replica Set name.
 */
class ReplicaSetNameToStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return 0);
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return 0;
    }
    return rs_info->start_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a Replica Set from its name.")
        .Details(
            "Gets the start time (in nanosecond unix time format) of a Replica Set from its name.")
        .Example("df.rs_start_time = px.replicaset_name_to_start_time(replica_set_name)")
        .Arg("replica_set_name",
             "The name of the Replica Set to get the name for. The name includes the namespace of "
             "the Replica Set. i.e. \"ns/rs_name\"")
        .Returns("The start time (as an integer) for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Replica Set stop time for Replica Set name.
 */
class ReplicaSetNameToStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return 0);
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return 0;
    }
    return rs_info->stop_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a Replica Set from its name.")
        .Details(
            "Gets the stop time (in nanosecond unix time format) of a Replica Set from its name.")
        .Example("df.rs_stop_time = px.replicaset_name_to_stop_time(replica_set_name)")
        .Arg("replica_set_name",
             "The name of the Replica Set to get the name for. The name includes the namespace of "
             "the Replica Set. i.e. \"ns/rs_name\"")
        .Returns("The stop time (as an integer) for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Replica Set namespace for Replica Set name.
 */
class ReplicaSetNameToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return "");
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }
    return rs_info->ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the namespace of a Replica Set from its name.")
        .Details("Gets the namespace of a Replica Set from its name.")
        .Example("df.namespace = px.replicaset_name_to_namespace(replica_set_name)")
        .Arg("replica_set_name",
             "The name of the Replica Set to get the name for. The name includes the namespace of "
             "the Replica Set. i.e. \"ns/rs_name\"")
        .Returns("The namespace for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Replica Set owner references for Replica Set name.
 */
class ReplicaSetNameToOwnerReferencesUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return "");
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    std::vector<std::string> owner_references;
    for (const auto& owner_reference : rs_info->owner_references()) {
      owner_references.push_back(OwnerReferenceString(owner_reference));
    }

    return VectorToStringArray(owner_references);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the owner references of a Replica Set from its name.")
        .Details("Gets the owner references of a Replica Set from its name.")
        .Example("df.owner_references = px.replicaset_name_to_owner_references(replica_set_name)")
        .Arg("replica_set_name",
             "The name of the Replica Set to get the name for. The name includes the namespace of "
             "the Replica Set. i.e. \"ns/rs_name\"")
        .Returns("The owner references for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Replica Set status for Replica Set name.
 */
class ReplicaSetNameToStatusUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return "");
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    return ReplicaSetInfoToStatus(rs_info);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the status of a Replica Set from its name.")
        .Details("Gets the status of a Replica Set from its name.")
        .Example("df.owner_references = px.replicaset_name_to_status(replica_set_name)")
        .Arg("replica_set_name",
             "The name of the Replica Set to get the name for. The name includes the namespace of "
             "the Replica Set. i.e. \"ns/rs_name\"")
        .Returns("The status for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Deployment name for Replica Set name.
 */
class ReplicaSetNameToDeploymentNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return "");
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", dep_info->ns(), dep_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment name of a Replica Set from its name.")
        .Details("Gets the Deployment name of a Replica Set from its name.")
        .Example("df.deployment_name = px.replicaset_name_to_deployment_name(replica_set_name)")
        .Arg("replica_set_name",
             "The Replica Set name of the Replica Set to get the Deployment name for.")
        .Returns("The Deployment name for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Deployment ID for Replica Set ID.
 */
class ReplicaSetNameToDeploymentIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue replica_set_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Replica Set name to be in the format of "<ns>/<replica-set-name>".
    PX_ASSIGN_OR(auto rs_name_view, internal::K8sName(replica_set_name), return "");
    auto replica_set_id = md->k8s_metadata_state().ReplicaSetIDByName(rs_name_view);

    auto rs_info = md->k8s_metadata_state().ReplicaSetInfoByID(replica_set_id);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return dep_info->uid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment ID of a Replica Set from its name.")
        .Details("Gets the Deployment ID of a Replica Set from its name.")
        .Example("df.deployment_id = px.replicaset_name_to_deployment_id(replica_set_name)")
        .Arg("replica_set_name",
             "The Replica Set name of the Replica Set to get the Deployment ID for.")
        .Returns("The Deployment ID for the Replica Set name passed in.");
  }
};

/**
 * @brief Returns the Deployment name for the given Deployment ID.
 */
class DeploymentIDToDeploymentNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue deployment_id) {
    auto md = GetMetadataState(ctx);
    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", dep_info->ns(), dep_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment Name from a Deployment ID.")
        .Details(
            "Gets the Kubernetes Deployment Name for the Deployment ID."
            "If the given ID doesn't have an associated Kubernetes Deployment, this function "
            "returns "
            "an empty string")
        .Example("df.deployment_name = px.deployment_id_to_deployment_name(deployment_id)")
        .Arg("deployment_id", "The ID of the Deployment to get the name for.")
        .Returns("The Kubernetes Deployment Name for the ID passed in.");
  }
};

/**
 * @brief Returns the Deployment start time for Deployment ID.
 */
class DeploymentIDToStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue deployment_id) {
    auto md = GetMetadataState(ctx);
    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return 0;
    }
    return dep_info->start_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a Deployment from its ID.")
        .Details(
            "Gets the start time (in nanosecond unix time format) of a Deployment from its ID.")
        .Example("df.deployment_start_time = px.deployment_id_to_start_time(deployment_id)")
        .Arg("deployment_id", "The Deployment ID of the Deployment to get the start time for.")
        .Returns("The start time (as an integer) for the Deployment ID passed in.");
  }
};

/**
 * @brief Returns the Deployment stop time for Deployment ID.
 */
class DeploymentIDToStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue deployment_id) {
    auto md = GetMetadataState(ctx);
    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return 0;
    }
    return dep_info->stop_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a Deployment from its ID.")
        .Details("Gets the stop time (in nanosecond unix time format) of a Deployment from its ID.")
        .Example("df.deployment_stop_time = px.deployment_id_to_stop_time(deployment_id)")
        .Arg("deployment_id", "The Deployment ID of the Deployment to get the stop time for.")
        .Returns("The stop time (as an integer) for the Deployment ID passed in.");
  }
};

/**
 * @brief Returns the Deployment namespace for Deployment ID.
 */
class DeploymentIDToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue deployment_id) {
    auto md = GetMetadataState(ctx);
    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return "";
    }
    return dep_info->ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the namespace of a Deployment from its ID.")
        .Details("Gets the namespace of a Deployment from its ID.")
        .Example("df.namespace = px.deployment_id_id_to_namespace(deployment_id)")
        .Arg("deployment_id", "The Deployment ID of the Deployment to get the namespace for.")
        .Returns("The namespace for the Deployment ID passed in.");
  }
};

/**
 * @brief Returns the Deployment status for Deployment ID.
 */
class DeploymentIDToStatusUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue deployment_id) {
    auto md = GetMetadataState(ctx);
    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return "";
    }

    return DeploymentInfoToStatus(dep_info);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the status of a Deployment from its ID.")
        .Details("Gets the status of a Deployment from its ID.")
        .Example("df.status = px.deployment_id_to_status(deployment_id)")
        .Arg("deployment_id", "The Deployment ID of the Deployment to get the status for.")
        .Returns("The status for the Deployment ID passed in.");
  }
};

/**
 * @brief Returns the Deployment ID from Deployment name.
 */
class DeploymentNameToDeploymentIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue deployment_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Deployment name to be in the format of "<ns>/<deployment-name>".
    PX_ASSIGN_OR(auto deployment_name_view, internal::K8sName(deployment_name), return "");
    auto deployment_id = md->k8s_metadata_state().DeploymentIDByName(deployment_name_view);

    return deployment_id;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment ID from a Deployment name.")
        .Details(
            "Gets the Kubernetes Deployment ID for the Deployment name."
            "If the given name doesn't have an associated Kubernetes Deployment, this function "
            "returns "
            "an empty string")
        .Example("df.deployment_id = px.deployment_name_to_deployment_id(deployment_name)")
        .Arg("deployment_name",
             "The name of the Deployment to get the ID for. The name includes the namespace of "
             "the Deployment. i.e. \"ns/deployment_name\"")
        .Returns("The Kubernetes Deployment ID for the name passed in.");
  }
};

/**
 * @brief Returns the Deployment start time for Deployment name.
 */
class DeploymentNameToStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue deployment_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Deployment name to be in the format of "<ns>/<deployment-name>".
    PX_ASSIGN_OR(auto deployment_name_view, internal::K8sName(deployment_name), return 0);
    auto deployment_id = md->k8s_metadata_state().DeploymentIDByName(deployment_name_view);

    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return 0;
    }
    return dep_info->start_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a Deployment from its name.")
        .Details(
            "Gets the start time (in nanosecond unix time format) of a Deployment from its name.")
        .Example("df.deployment_start_time = px.deployment_id_to_start_time(deployment_name)")
        .Arg("deployment_name",
             "The name of the Deployment to get the name for. The name includes the namespace of "
             "the Deployment. i.e. \"ns/deployment_name\"")
        .Returns("The start time (as an integer) for the Deployment name passed in.");
  }
};

/**
 * @brief Returns the Deployment stop time for Deployment name.
 */
class DeploymentNameToStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue deployment_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Deployment name to be in the format of "<ns>/<deployment-name>".
    PX_ASSIGN_OR(auto deployment_name_view, internal::K8sName(deployment_name), return 0);
    auto deployment_id = md->k8s_metadata_state().DeploymentIDByName(deployment_name_view);

    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return 0;
    }
    return dep_info->stop_time_ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a Deployment from its name.")
        .Details(
            "Gets the stop time (in nanosecond unix time format) of a Deployment from its name.")
        .Example("df.deployment_stop_time = px.deployment_name_to_stop_time(deployment_name)")
        .Arg("deployment_name",
             "The name of the Deployment to get the name for. The name includes the namespace of "
             "the Deployment. i.e. \"ns/deployment_name\"")
        .Returns("The stop time (as an integer) for the Deployment name passed in.");
  }
};

/**
 * @brief Returns the Deployment namespace for Deployment name.
 */
class DeploymentNameToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue deployment_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Deployment name to be in the format of "<ns>/<deployment-name>".
    PX_ASSIGN_OR(auto deployment_name_view, internal::K8sName(deployment_name), return "");
    auto deployment_id = md->k8s_metadata_state().DeploymentIDByName(deployment_name_view);

    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return "";
    }
    return dep_info->ns();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the namespace of a Deployment from its name.")
        .Details("Gets the namespace of a Deployment from its name.")
        .Example("df.namespace = px.deployment_name_to_namespace(deployment_name)")
        .Arg("deployment_name",
             "The name of the Deployment to get the name for. The name includes the namespace of "
             "the Deployment. i.e. \"ns/deployment_name\"")
        .Returns("The namespace for the Deployment name passed in.");
  }
};

/**
 * @brief Returns the Deployment status for Deployment name.
 */
class DeploymentNameToStatusUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue deployment_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the Deployment name to be in the format of "<ns>/<deployment-name>".
    PX_ASSIGN_OR(auto deployment_name_view, internal::K8sName(deployment_name), return "");
    auto deployment_id = md->k8s_metadata_state().DeploymentIDByName(deployment_name_view);

    auto dep_info = md->k8s_metadata_state().DeploymentInfoByID(deployment_id);
    if (dep_info == nullptr) {
      return "";
    }

    return DeploymentInfoToStatus(dep_info);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the status of a Deployment from its name.")
        .Details("Gets the status of a Deployment from its name.")
        .Example("df.status = px.deployment_id_to_status(deployment_name)")
        .Arg("deployment_name",
             "The name of the Deployment to get the name for. The name includes the namespace of "
             "the Deployment. i.e. \"ns/deployment_name\"")
        .Returns("The status for the Deployment name passed in.");
  }
};

/**
 * @brief Returns the Replica Set names for Replica Sets that are currently running.
 */
class UPIDToReplicaSetNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);

    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    if (rs_info->stop_time_ns() != 0) {
      return "";
    }
    return absl::Substitute("$0/$1", rs_info->ns(), rs_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Replica Set Name from a UPID.")
        .Details(
            "Gets the Kubernetes Replica Set Name for the process with the given Unique Process ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes Replica Set, this function "
            "returns "
            "an empty string")
        .Example("df.replica_set_name = px.upid_to_replicaset_name(df.upid)")
        .Arg("upid", "The UPID of the process to get the service name for.")
        .Returns("The Kubernetes Replica Set Name for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the Replica Set IDs for Replica Sets that are currently running.
 */
class UPIDToReplicaSetIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    if (rs_info->stop_time_ns() != 0) {
      return "";
    }
    return rs_info->uid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Replica Set ID from a UPID.")
        .Details(
            "Gets the Kubernetes Replica Set ID for the process with the given Unique Process ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes Replica Set, this function "
            "returns "
            "an empty string.")
        .Example("df.replica_set_id = px.upid_to_replicaset_id(df.upid)")
        .Arg("upid", "The UPID of the process to get the service ID for.")
        .Returns("The Kubernetes Replica Set ID for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the Replica Set status for Replica Sets that are currently running.
 */
class UPIDToReplicaSetStatusUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    return ReplicaSetInfoToStatus(rs_info);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Replica Set Status from a UPID.")
        .Details(
            "Gets the Kubernetes Replica Set Status for the process with the given Unique Process "
            "ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes Replica Set, this function "
            "returns "
            "an empty string.")
        .Example("df.replica_set_status = px.upid_to_replica_set_status(df.upid)")
        .Arg("upid", "The UPID of the process to get the Replica Set ID for.")
        .Returns("The Kubernetes Replica Set Status for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the Deployment name for processes which are currently running.
 */
class UPIDToDeploymentNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);

    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    if (dep_info->stop_time_ns() != 0) {
      return "";
    }
    return absl::Substitute("$0/$1", dep_info->ns(), dep_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment Name from a UPID.")
        .Details(
            "Gets the Kubernetes Deployment Name for the process with the given Unique Process ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes Deployment, this function "
            "returns "
            "an empty string")
        .Example("df.deployment_name = px.upid_to_deployment_name(df.upid)")
        .Arg("upid", "The UPID of the process to get the Deployment name for.")
        .Returns("The Kubernetes Deployment Name for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the Deployment ID for process which is currently running.
 */
class UPIDToDeploymentIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    if (dep_info->stop_time_ns() != 0) {
      return "";
    }
    return dep_info->uid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Deployment ID from a UPID.")
        .Details(
            "Gets the Kubernetes Deployment ID for the process with the given Unique Process ID "
            "(UPID). "
            "If the given process doesn't have an associated Kubernetes Deployment, this function "
            "returns "
            "an empty string.")
        .Example("df.deployment_id = px.upid_to_deployment_id(df.upid)")
        .Arg("upid", "The UPID of the process to get the Deployment ID for.")
        .Returns("The Kubernetes Deployment ID for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the hostname for the pod associated with the input upid.
 */
class UPIDToHostnameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto pod_info = UPIDtoPod(md, upid_value);
    if (pod_info == nullptr) {
      return "";
    }
    return pod_info->hostname();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Hostname from a UPID.")
        .Details(
            "Gets the name of the host the process with the given Unique Process ID (UPID) is "
            "running on. "
            "Equivalent to running `hostname` in a shell on the node the process is running on.")
        .Example("df.hostname = px.upid_to_hostname(df.upid)")
        .Arg("upid", "The UPID of the process to get the hostname for.")
        .Returns("The hostname for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

/**
 * @brief Returns the service names for the given pod ID.
 */
class PodIDToServiceNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    std::vector<std::string> running_service_names;
    for (const auto& service_id : pod_info->services()) {
      auto service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
      if (service_info == nullptr) {
        continue;
      }
      if (service_info->stop_time_ns() == 0) {
        running_service_names.push_back(
            absl::Substitute("$0/$1", service_info->ns(), service_info->name()));
      }
    }
    return StringifyVector(running_service_names);
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::ExplicitRule::Create<PodIDToServiceNameUDF>(types::ST_SERVICE_NAME, {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the service name for a given pod ID.")
        .Details(
            "Gets the Kubernetes service name for the service associated to the pod. If there is "
            "no "
            "service associated to this pod, then this function returns an empty string.")
        .Example("df.service_name = px.pod_id_to_service_name(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get service name for.")
        .Returns("The k8s service name for the Pod ID passed in.");
  }
};

/**
 * @brief Returns the service ids for the given pod ID.
 */
class PodIDToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    std::vector<std::string> running_service_ids;
    for (const auto& service_id : pod_info->services()) {
      auto service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
      if (service_info == nullptr) {
        continue;
      }
      if (service_info->stop_time_ns() == 0) {
        running_service_ids.push_back(service_id);
      }
    }
    return StringifyVector(running_service_ids);
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the service ID for a given pod ID.")
        .Details(
            "Gets the Kubernetes service ID for the service associated to the pod. If there is no "
            "service associated to this pod, then this function returns an empty string.")
        .Example("df.service_id = px.pod_id_to_service_id(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get service ID for.")
        .Returns("The k8s service ID for the Pod ID passed in.");
  }
};

/**
 * @brief Returns the owner references for the given pod ID.
 */
class PodIDToOwnerReferencesUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    std::vector<std::string> owner_references;
    for (const auto& owner_reference : pod_info->owner_references()) {
      owner_references.push_back(OwnerReferenceString(owner_reference));
    }
    return VectorToStringArray(owner_references);
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the owner references for a given pod ID.")
        .Details(
            "Gets the owner references for the pod. If there is no "
            "owner references associated to this pod, then this function returns an empty string.")
        .Example("df.owner_references = px.pod_id_to_owner_references(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get owner references for.")
        .Returns("The k8s owner references for the Pod ID passed in.");
  }
};

/**
 * @brief Returns the owner references for the given pod name.
 */
class PodNameToOwnerReferencesUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    std::vector<std::string> owner_references;
    for (const auto& owner_reference : pod_info->owner_references()) {
      owner_references.push_back(OwnerReferenceString(owner_reference));
    }
    return VectorToStringArray(owner_references);
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the owner references for a given pod name.")
        .Details(
            "Gets the owner references for the pod (specified by the pod_name). If there is no "
            "owner references associated to this pod, then this function returns an empty string.")
        .Example("df.owner_references = px.pod_name_to_owner_references(df.pod_name)")
        .Arg("pod_name", "The Pod name of the Pod to get service ID for.")
        .Returns("The k8s owner references for the Pod name passed in.");
  }
};

/**
 * @brief Returns the Node Name of a pod ID passed in.
 */
class PodIDToNodeNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }
    std::string foo = std::string(pod_info->node_name());
    return foo;
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<PodIDToNodeNameUDF>(types::ST_NODE_NAME, {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the name of the node a pod ID is running on.")
        .Details(
            "Gets the Kubernetes name for the node that the Pod (specified by Pod ID) is running "
            "on.")
        .Example("df.node_name = px.pod_id_to_node_name(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the node name for.")
        .Returns("The k8s node name for the Pod ID passed in.");
  }
};

/**
 * @brief Returns the ReplicaSet name of a pod ID passed in.
 */
class PodIDToReplicaSetNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", rs_info->ns(), rs_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the name of the Replica Set which controls the pod with pod ID.")
        .Details(
            "Gets the Kubernetes name for the Replica Set that owns the Pod (specified by Pod ID)."
            "If this pod is not controlled by any Replica Set, returns an empty string.")
        .Example("df.replicaset_name = px.pod_id_to_replicaset_name(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the Replica Set name for.")
        .Returns("The k8s Replica Set name wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the ReplicaSet ID of a pod ID passed in.
 */
class PodIDToReplicaSetIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    return rs_info->uid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the ID of the Replica Set which controls the pod with pod ID.")
        .Details(
            "Gets the Kubernetes ID for the Replica Set that owns the Pod (specified by Pod ID)."
            "If this pod is not controlled by any Replica Set, returns an empty string.")
        .Example("df.replicaset_id = px.pod_id_to_replicaset_id(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the Replica Set ID for.")
        .Returns("The k8s Replica Set ID wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the Deployment name of a pod ID passed in.
 */
class PodIDToDeploymentNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", dep_info->ns(), dep_info->name());
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the name of the Deployment which controls the pod with pod ID.")
        .Details(
            "Gets the Kubernetes name for the Deployment that owns the Pod (specified by Pod ID)."
            "If this pod is not controlled by any Deployment, returns an empty string.")
        .Example("df.replicaset_name = px.pod_id_to_deployment_name(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the Deployment name for.")
        .Returns("The k8s Deployment name wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the Deployment ID of a pod ID passed in.
 */
class PodIDToDeploymentIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return dep_info->uid();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the ID of the Deployment which controls the pod with pod ID.")
        .Details(
            "Gets the Kubernetes ID for the Deployment that owns the Pod (specified by Pod ID)."
            "If this pod is not controlled by any Deployment, returns an empty string.")
        .Example("df.deployment_id = px.pod_id_to_deployment_id(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the Deployment ID for.")
        .Returns("The k8s Deployment ID wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the ReplicaSet name of a pod name passed in.
 */
class PodNameToReplicaSetNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", rs_info->ns(), rs_info->name());
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the name of the Replica Set which controls the pod with the specified pod "
               "name.")
        .Details(
            "Gets the Kubernetes name for the Replica Set that owns the Pod (specified by pod "
            "name)."
            "If this pod is not controlled by any Replica Set, returns an empty string.")
        .Example("df.replica_set_name = px.pod_name_to_replicaset_name(df.pod_name)")
        .Arg("pod_name", "The Pod name of the Pod to get the Replica Set name for.")
        .Returns("The k8s Replica Set name wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the ReplicaSet ID of a Pod name passed in.
 */
class PodNameToReplicaSetIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    return rs_info->uid();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the ID of the Replica Set which controls the pod with the specified pod "
               "name.")
        .Details(
            "Gets the Kubernetes ID for the Replica Set that owns the Pod (specified by pod "
            "name)."
            "If this pod is not controlled by any Replica Set, returns an empty string.")
        .Example("df.replica_set_id = px.pod_name_to_replicaset_id(df.pod_name)")
        .Arg("pod_name", "The Pod name of the Pod to get the Replica Set ID for.")
        .Returns("The k8s Replica Set ID wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the Deployment name of a pod name passed in.
 */
class PodNameToDeploymentNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return absl::Substitute("$0/$1", dep_info->ns(), dep_info->name());
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the name of the Deployment which controls the pod with the specified pod "
               "name.")
        .Details(
            "Gets the Kubernetes name for the Deployment that owns the Pod (specified by pod "
            "name)."
            "If this pod is not controlled by any Deployment, returns an empty string.")
        .Example("df.replica_set_name = px.pod_name_to_deployment_name(df.pod_name)")
        .Arg("pod_name", "The Pod name of the Pod to get the Deployment name for.")
        .Returns("The k8s Deployment name wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the Deployment ID of a Pod name passed in.
 */
class PodNameToDeploymentIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    auto rs_info = md->k8s_metadata_state().OwnerReplicaSetInfo(pod_info);
    if (rs_info == nullptr) {
      return "";
    }

    auto dep_info = md->k8s_metadata_state().OwnerDeploymentInfo(rs_info);
    if (dep_info == nullptr) {
      return "";
    }

    return dep_info->uid();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Get the ID of the Deployment which controls the pod with the specified pod "
               "name.")
        .Details(
            "Gets the Kubernetes ID for the Deployment that owns the Pod (specified by pod "
            "name)."
            "If this pod is not controlled by any Deployment, returns an empty string.")
        .Example("df.replica_set_id = px.pod_name_to_deployment_id(df.pod_name)")
        .Arg("pod_name", "The Pod name of the Pod to get the Deployment ID for.")
        .Returns("The k8s Deployment ID wich controls the Pod with the Pod ID.");
  }
};

/**
 * @brief Returns the service names for the given pod name.
 */
class PodNameToServiceNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    std::vector<std::string> running_service_names;
    for (const auto& service_id : pod_info->services()) {
      auto service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
      if (service_info == nullptr) {
        continue;
      }
      if (service_info->stop_time_ns() == 0) {
        running_service_names.push_back(
            absl::Substitute("$0/$1", service_info->ns(), service_info->name()));
      }
    }
    return StringifyVector(running_service_names);
  }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<PodNameToServiceNameUDF>(types::ST_SERVICE_NAME,
                                                               {types::ST_POD_NAME})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the service name for a given pod name.")
        .Details(
            "Gets the Kubernetes service name for the service associated to the pod. If there is "
            "no "
            "service associated to this pod, then this function returns an empty string.")
        .Example("df.service_name = px.pod_name_to_service_name(df.pod_name)")
        .Arg("pod_name", "The name of the Pod to get service name for.")
        .Returns("The k8s service name for the Pod name passed in.");
  }
};

/**
 * @brief Returns the service ids for the given pod name.
 */
class PodNameToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PX_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }

    std::vector<std::string> running_service_ids;
    for (const auto& service_id : pod_info->services()) {
      auto service_info = md->k8s_metadata_state().ServiceInfoByID(service_id);
      if (service_info == nullptr) {
        continue;
      }
      if (service_info->stop_time_ns() == 0) {
        running_service_ids.push_back(service_id);
      }
    }
    return StringifyVector(running_service_ids);
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the service ID for a given pod name.")
        .Details(
            "Gets the Kubernetes service ID for the service associated to the pod. If there is no "
            "service associated to this pod, then this function returns an empty string.")
        .Example("df.service_id = px.pod_name_to_service_id(df.pod_name)")
        .Arg("pod_id", "The name of the Pod to get service ID for.")
        .Returns("The k8s service ID for the Pod name passed in.");
  }
};

class UPIDToStringUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, UInt128Value upid_value) {
    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    return upid.String();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get a stringified version of the UPID.")
        .Details(
            "Stringifies a UPID "
            "The string format of the UPID is `asid:pid:start_time`, where asid is the "
            "Pixie Agent unique ID "
            "that uniquely determines which Pixie Agent traces this UPID, pid is the process ID "
            "from the host, and "
            "start_time is the unix time the process started.")
        .Example("df.upid_str = px.upid_to_string(df.upid)")
        .Arg("upid", "The UPID to stringify.")
        .Returns("The stringified UPID.");
  }
};

class UPIDToPIDUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, UInt128Value upid_value) {
    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    return static_cast<int64_t>(upid.pid());
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the PID of the process for the given UPID.")
        .Details(
            "Get the process ID for the process the Unique Process ID (UPID) refers to. "
            "Note that the UPID is unique across all hosts/containers, whereas the PID could be "
            "the same "
            "between different hosts/containers")
        .Example("df.pid = px.upid_to_pid(df.upid)")
        .Arg("upid", "The UPID of the process to get the PID for.")
        .Returns("The PID for the UPID passed in.");
  }
};

class UPIDToStartTSUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext*, UInt128Value upid_value) {
    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    return static_cast<int64_t>(upid.start_ts());
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the starting timestamp of the process for the given UPID.")
        .Details(
            "Get the starting timestamp for the process referred to by the Unique Process ID "
            "(UPID). ")
        .Example("df.timestamp = px.upid_to_start_ts(df.upid)")
        .Arg("upid", "The unique process ID of the process to get the starting timestamp for.")
        .Returns("The starting timestamp for the UPID passed in.");
  }
};

class PodIDToPodStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);
    const px::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return 0;
    }
    return pod_info->start_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a pod from its ID.")
        .Details("Gets the start time (in nanosecond unix time format) of a pod from its pod ID.")
        .Example("df.pod_start_time = px.pod_id_to_start_time(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the start time for.")
        .Returns("The start time (as an integer) for the Pod ID passed in.");
  }
};

class PodIDToPodStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);
    const px::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return 0;
    }
    return pod_info->stop_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a pod from its ID.")
        .Details("Gets the stop time (in nanosecond unix time format) of a pod from its pod ID.")
        .Example("df.pod_stop_time = px.pod_id_to_stop_time(df.pod_id)")
        .Arg("pod_id", "The Pod ID of the Pod to get the stop time for.")
        .Returns("The stop time (as an integer) for the Pod ID passed in.");
  }
};

class PodNameToPodStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    const px::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return 0;
    }
    return pod_info->start_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a pod from its name.")
        .Details("Gets the start time (in nanosecond unix time format) of a pod from its name.")
        .Example("df.pod_start_time = px.pod_name_to_start_time(df.pod_name)")
        .Arg("pod_name", "The name of the Pod to get the start time for.")
        .Returns("The start time (as an integer) for the Pod name passed in.");
  }
};

class PodNameToPodStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    const px::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return 0;
    }
    return pod_info->stop_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a pod from its name.")
        .Details("Gets the stop time (in nanosecond unix time format) of a pod from its name.")
        .Example("df.pod_stop_time = px.pod_name_to_stop_time(df.pod_name)")
        .Arg("pod_name", "The name of the Pod to get the stop time for.")
        .Returns("The stop time (as an integer) for the Pod name passed in.");
  }
};

class ContainerNameToContainerIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue container_name) {
    auto md = GetMetadataState(ctx);
    return md->k8s_metadata_state().ContainerIDByName(container_name);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the id of a container from its name.")
        .Details("Gets the kubernetes ID for the container from its name.")
        .Example("df.container_id = px.container_name_to_container_id(df.container_name)")
        .Arg("container_name", "The name of the container to get the ID for.")
        .Returns("The k8s container ID for the container name passed in.");
  }
};

class ContainerIDToContainerStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue container_id) {
    auto md = GetMetadataState(ctx);
    const px::md::ContainerInfo* container_info =
        md->k8s_metadata_state().ContainerInfoByID(container_id);
    if (container_info == nullptr) {
      return 0;
    }
    return container_info->start_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a container from its ID.")
        .Details(
            "Gets the start time (in nanosecond unix time format) of a container from its "
            "container ID.")
        .Example("df.container_start_time = px.container_id_to_start_time(df.container_id)")
        .Arg("container_id", "The Container ID of the Container to get the start time for.")
        .Returns("The start time (as an integer) for the Container ID passed in.");
  }
};

class ContainerIDToContainerStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue container_id) {
    auto md = GetMetadataState(ctx);
    const px::md::ContainerInfo* container_info =
        md->k8s_metadata_state().ContainerInfoByID(container_id);
    if (container_info == nullptr) {
      return 0;
    }
    return container_info->stop_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a container from its ID.")
        .Details(
            "Gets the stop time (in nanosecond unix time format) of a container from its container "
            "ID.")
        .Example("df.container_stop_time = px.container_id_to_stop_time(df.container_id)")
        .Arg("container_id", "The Container ID of the Container to get the stop time for.")
        .Returns("The stop time (as an integer) for the Container ID passed in.");
  }
};

class ContainerNameToContainerStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue container_name) {
    auto md = GetMetadataState(ctx);
    StringValue container_id = md->k8s_metadata_state().ContainerIDByName(container_name);
    const px::md::ContainerInfo* container_info =
        md->k8s_metadata_state().ContainerInfoByID(container_id);
    if (container_info == nullptr) {
      return 0;
    }
    return container_info->start_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the start time of a container from its name.")
        .Details(
            "Gets the start time (in nanosecond unix time format) of a container from its name.")
        .Example("df.container_start_time = px.container_name_to_start_time(df.container_name)")
        .Arg("container_name", "The name of the Container to get the start time for.")
        .Returns("The start time (as an integer) for the Container name passed in.");
  }
};

class ContainerNameToContainerStopTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue container_name) {
    auto md = GetMetadataState(ctx);
    StringValue container_id = md->k8s_metadata_state().ContainerIDByName(container_name);
    const px::md::ContainerInfo* container_info =
        md->k8s_metadata_state().ContainerInfoByID(container_id);
    if (container_info == nullptr) {
      return 0;
    }
    return container_info->stop_time_ns();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the stop time of a container from its name.")
        .Details(
            "Gets the stop time (in nanosecond unix time format) of a container from its name.")
        .Example("df.container_stop_time = px.container_name_to_stop_time(df.container_name)")
        .Arg("container_name", "The name of the Container to get the stop time for.")
        .Returns("The stop time (as an integer) for the Container name passed in.");
  }
};

inline std::string PodPhaseToString(const px::md::PodPhase& pod_phase) {
  switch (pod_phase) {
    case md::PodPhase::kRunning:
      return "Running";
    case md::PodPhase::kPending:
      return "Pending";
    case md::PodPhase::kSucceeded:
      return "Succeeded";
    case md::PodPhase::kFailed:
      return "Failed";
    case md::PodPhase::kTerminated:
      return "Terminated";
    case md::PodPhase::kUnknown:
    default:
      return "Unknown";
  }
}

inline types::StringValue PodInfoToPodStatus(const px::md::PodInfo* pod_info) {
  std::string phase = PodPhaseToString(md::PodPhase::kUnknown);
  std::string msg = "";
  std::string reason = "";
  bool ready_condition = false;

  if (pod_info != nullptr) {
    phase = PodPhaseToString(pod_info->phase());
    msg = pod_info->phase_message();
    reason = pod_info->phase_reason();

    auto pod_conditions = pod_info->conditions();
    auto ready_status = pod_conditions.find(md::PodConditionType::kReady);
    ready_condition = ready_status != pod_conditions.end() &&
                      (ready_status->second == md::ConditionStatus::kTrue);
  }

  rapidjson::Document d;
  d.SetObject();
  d.AddMember("phase", internal::StringRef(phase), d.GetAllocator());
  d.AddMember("message", internal::StringRef(msg), d.GetAllocator());
  d.AddMember("reason", internal::StringRef(reason), d.GetAllocator());
  d.AddMember("ready", ready_condition, d.GetAllocator());
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  d.Accept(writer);
  return sb.GetString();
}

class PodNameToPodStatusUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the Pod status for a passed in pod.
   *
   * @param ctx: the function context
   * @param pod_name: the Value containing a pod name.
   * @return StringValue: the status of the pod.
   */
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    auto pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    return PodInfoToPodStatus(pod_info);
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::ExplicitRule::Create<PodNameToPodStatusUDF>(types::ST_POD_STATUS, {types::ST_NONE})};
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get status information about the given pod.")
        .Details(
            "Gets the Kubernetes status information for the pod with the given name. "
            "The status is a subset of the Kubernetes PodStatus object returned as JSON. "
            "The keys included are state, message, and reason. "
            "See "
            "https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.18/"
            "#podstatus-v1-core "
            "for more info about this object. ")
        .Example("df.pod_status = px.pod_name_to_status(df.pod_name)")
        .Arg("pod_name", "The name of the pod to get the PodStatus for.")
        .Returns("The Kubernetes PodStatus for the Pod passed in.");
  }
};

class PodNameToPodReadyUDF : public ScalarUDF {
 public:
  BoolValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    auto pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return false;
    }
    auto pod_conditions = pod_info->conditions();
    auto ready_status = pod_conditions.find(md::PodConditionType::kReady);
    if (ready_status == pod_conditions.end()) {
      return false;
    }
    return ready_status->second == md::ConditionStatus::kTrue;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get readiness information about the given pod.")
        .Details(
            "Gets the Kubernetes service ready state of pod. "
            "The ready state is truthy is the pod ready condition is true."
            "If the pod ready condition is false or unknown, this will return false."
            "See "
            "https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle#pod-conditions"
            "for more info about how we determine this. ")
        .Example("df.pod_read = px.pod_name_to_ready(df.pod_name)")
        .Arg("pod_name", "The name of the pod to get the Pod readiness for.")
        .Returns(
            "A value denoting whether the service state of the pod passed in is ready or not.");
  }
};

class PodNameToPodStatusMessageUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the Pod status message for a passed in pod.
   *
   * @param ctx: the function context
   * @param pod_name: the Value containing a pod name.
   * @return StringValue: the status message of the pod.
   */
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    auto pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }
    return pod_info->phase_message();
  }
};

class PodNameToPodStatusReasonUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the Pod status reason for a passed in pod.
   *
   * @param ctx: the function context
   * @param pod_name: the Value containing a pod name.
   * @return StringValue: the status reason of the pod.
   */
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    auto pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return "";
    }
    return pod_info->phase_reason();
  }
};

inline std::string ContainerStateToString(const px::md::ContainerState& container_state) {
  switch (container_state) {
    case md::ContainerState::kRunning:
      return "Running";
    case md::ContainerState::kWaiting:
      return "Waiting";
    case md::ContainerState::kTerminated:
      return "Terminated";
    case md::ContainerState::kUnknown:
    default:
      return "Unknown";
  }
}

class ContainerIDToContainerStatusUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the Container status for a passed in container.
   *
   * @param ctx: the function context
   * @param container_id: the Value containing a container ID.
   * @return StringValue: the status of the container.
   */
  StringValue Exec(FunctionContext* ctx, StringValue container_id) {
    auto md = GetMetadataState(ctx);
    auto container_info = md->k8s_metadata_state().ContainerInfoByID(container_id);

    std::string state = ContainerStateToString(md::ContainerState::kUnknown);
    std::string msg = "";
    std::string reason = "";
    if (container_info != nullptr) {
      state = ContainerStateToString(container_info->state());
      msg = container_info->state_message();
      reason = container_info->state_reason();
    }
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("state", internal::StringRef(state), d.GetAllocator());
    d.AddMember("message", internal::StringRef(msg), d.GetAllocator());
    d.AddMember("reason", internal::StringRef(reason), d.GetAllocator());
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    d.Accept(writer);
    return sb.GetString();
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<ContainerIDToContainerStatusUDF>(types::ST_CONTAINER_STATUS,
                                                                       {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the status of the container given the container ID.")
        .Details(
            "Get the status of the container given the container ID. The status is the K8s state "
            "of either 'Running', 'Waiting', 'Terminated' or 'Unknown'. It may be paired with "
            "additional information such as a message and reason explaining why the container is "
            "in that state.")
        .Arg("id", "The ID of the container to get the status of.")
        .Example("df.status = px.container_id_to_status(df.id)")
        .Returns("The status of the container.");
  }
};

class UPIDToPodStatusUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the Pod status for a passed in UPID.
   *
   * @param ctx: the function context
   * @param upid_vlue: the UPID to query for.
   * @return StringValue: the status of the pod.
   */
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    return PodInfoToPodStatus(UPIDtoPod(md, upid_value));
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<UPIDToPodStatusUDF>(types::ST_POD_STATUS, {types::ST_NONE})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get status information about the pod of a UPID.")
        .Details(
            "Gets the Kubernetes status information for the pod the given Unique Process ID (UPID) "
            "is running on. "
            "The status is a subset of the Kubernetes PodStatus object returned as JSON. "
            "The keys included are state, message, and reason. "
            "See "
            "https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.18/"
            "#podstatus-v1-core "
            "for more info about this object. "
            "If the UPID has no associated kubernetes pod, this will return an empty string.")
        .Example("df.pod_status = px.upid_to_pod_status(df.upid)")
        .Arg("upid", "The UPID to get the PodStatus for.")
        .Returns("The Kubernetes PodStatus for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

class UPIDToCmdLineUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the cmdline for the upid.
   *
   * @param ctx: The function context.
   * @param upid_value: The UPID value
   * @return StringValue: the cmdline for the UPID.
   */
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    auto pid_info = md->GetPIDByUPID(upid);
    if (pid_info == nullptr) {
      return "";
    }
    return pid_info->cmdline();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the command line arguments used to start a UPID.")
        .Details(
            "Get the command line arguments used to start the process with the given Unique "
            "Process ID (UPID).")
        .Example("df.cmdline = px.upid_to_cmdline(df.upid)")
        .Arg("upid", "The UPID to get the command line arguments for.")
        .Returns("The command line arguments for the UPID passed in, as a string.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

inline std::string PodInfoToPodQoS(const px::md::PodInfo* pod_info) {
  if (pod_info == nullptr) {
    return "";
  }
  return std::string(magic_enum::enum_name(pod_info->qos_class()));
}

class UPIDToPodQoSUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the qos for the upid's pod.
   *
   * @param ctx: The function context.
   * @param upid_value: The UPID value
   * @return StringValue: the cmdline for the UPID.
   */
  StringValue Exec(FunctionContext* ctx, UInt128Value upid_value) {
    auto md = GetMetadataState(ctx);
    return PodInfoToPodQoS(UPIDtoPod(md, upid_value));
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes QOS class for the UPID.")
        .Details(
            "Gets the Kubernetes QOS class for the pod the given Unique Process ID (UPID) is "
            "running on. "
            "The QOS Class is one of \"Guaranteed\", \"Burstable\", or \"BestEffort\". "
            "See https://kubernetes.io/docs/tasks/configure-pod-container/quality-service-pod/ for "
            "more info.")
        .Example("df.pod_qos = px.upid_to_pod_qos(df.upid)")
        .Arg("upid", "The UPID to get the Pod QOS class for.")
        .Returns("The Kubernetes Pod QOS class for the UPID passed in.");
  }

  // This UDF can currently only run on PEMs, because only PEMs have the UPID information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }
};

class HostnameUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the hostname of the machine.
   */
  StringValue Exec(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);

    return md->hostname();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the hostname of the machine.")
        .Details("Get the hostname of the machine that the data originated from.")
        .Example("df.hostname = px._exec_hostname()")
        .Returns("The hostname of the machine.");
  }
};

class HostNumCPUsUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the number of CPUs on the machine.
   */
  Int64Value Exec(FunctionContext* /* ctx */) {
    const size_t ncpus = get_nprocs_conf();
    return ncpus;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the number of CPUs on the host machine.")
        .Details("Get the number of CPUs on the machine where the function is executed.")
        .Example("df.num_cpus = px._exec_host_num_cpus()")
        .Returns("The number of CPUs of the host machine.");
  }
};

class IPToPodIDUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the pod id of pod with given pod_ip
   */
  StringValue Exec(FunctionContext* ctx, StringValue pod_ip) {
    auto md = GetMetadataState(ctx);
    return md->k8s_metadata_state().PodIDByIP(pod_ip);
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Convert IP address to the kubernetes pod ID that runs the backing service.")
        .Details(
            "Converts the IP address into a kubernetes pod ID for that IP address if it exists, "
            "otherwise returns an empty string. Converting to a pod ID means you can then extract "
            "the corresponding service name using `px.pod_id_to_service_name`.\n"
            "Note that this will not be able to convert IP addresses into DNS names generally as "
            "this is limited to internal Kubernetes state.")
        .Example(R"doc(
        | # Convert to the Kubernetes pod ID.
        | df.pod_id = px.ip_to_pod_id(df.remote_addr)
        | # Convert the ID to a readable name.
        | df.service = px.pod_id_to_service_name(df.pod_id)
        )doc")
        .Arg("pod_ip", "The IP of a pod to convert.")
        .Returns("The Kubernetes ID of the pod if it exists, otherwise an empty string.");
  }

  // This UDF can currently only run on Kelvins, because only Kelvins have the IP to pod
  // information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_KELVIN; }
};

class IPToPodIDAtTimeUDF : public ScalarUDF {
 public:
  /**
   * @brief Gets the pod id of pod with given pod_ip
   */
  StringValue Exec(FunctionContext* ctx, StringValue pod_ip, Time64NSValue time) {
    auto md = GetMetadataState(ctx);
    return md->k8s_metadata_state().PodIDByIPAtTime(pod_ip, time.val);
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Convert IP address to the kubernetes pod ID that runs the backing service "
               "given the time that the request was made.")
        .Details(
            "Converts the IP address into a kubernetes pod ID at the given time for that IP "
            "address if it exists, otherwise returns an empty string. Converting to a pod ID "
            "means you can then extract the corresponding service name using "
            "`px.pod_id_to_service_name`.\nNote that this will not be able to convert IP "
            "addresses into DNS names generally as this is limited to internal Kubernetes state.")
        .Example(R"doc(
        | # Convert to the Kubernetes pod ID.
        | df.pod_id = px.ip_to_pod_id(df.remote_addr, df.time_)
        | # Convert the ID to a readable name.
        | df.service = px.pod_id_to_service_name(df.pod_id)
        )doc")
        .Arg("pod_ip", "The IP of a pod to convert.")
        .Arg("time", "The time at which this trace was captured.")
        .Returns("The Kubernetes ID of the pod if it exists, otherwise an empty string.");
  }

  // This UDF can currently only run on Kelvins, because only Kelvins have the IP to pod
  // information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_KELVIN; }
};

class IPToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue ip) {
    auto md = GetMetadataState(ctx);
    // First, check the list of Service Cluster IPs for this IP.
    auto service_id = md->k8s_metadata_state().ServiceIDByClusterIP(ip);
    if (service_id != "") {
      return service_id;
    }
    // Next, check to see if the IP is in the list of of Pod IPs.
    auto pod_id = md->k8s_metadata_state().PodIDByIP(ip);
    if (pod_id == "") {
      return "";
    }
    PodIDToServiceIDUDF udf;
    return udf.Exec(ctx, pod_id);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the service ID for a given IP.")
        .Details(
            "Converts the IP address into a Kubernetes service ID for the service "
            "associated to the IP. If there is no service associated with the given IP address, "
            "return an empty string.")
        .Example("df.service_id = px.ip_to_service_id(df.remote_addr)")
        .Arg("ip", "The IP to convert.")
        .Returns("The service id if it exists, otherwise an empty string.");
  }

  // This UDF can currently only run on Kelvins, because only Kelvins have the IP to pod
  // information.
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_KELVIN; }
};

inline bool EqualsOrArrayContains(const std::string& input, const std::string& value) {
  rapidjson::Document doc;
  doc.Parse(input.c_str());
  if (!doc.IsArray()) {
    return input == value;
  }
  for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
    if (!doc[i].IsString()) {
      return false;
    }
    auto str = doc[i].GetString();
    if (str == value) {
      return true;
    }
  }
  return false;
}

class HasServiceNameUDF : public ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, StringValue service, StringValue value) {
    return EqualsOrArrayContains(service, value);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Determine if a particular service name is present")
        .Details(
            "Checks to see if a given service is present. Can include matching an individual "
            "service, or checking against a list of services.")
        .Example("df = df[px.has_service_name(df.ctx[\"service\"], \"kube-system/kube-dns\")]")
        .Arg("service", "The service to check")
        .Arg("value", "The value to check for in service")
        .Returns("True if value is present in service, otherwise false");
  }
};

class HasServiceIDUDF : public ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, StringValue service, StringValue value) {
    return EqualsOrArrayContains(service, value);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Determine if a particular service ID is present")
        .Details(
            "Checks to see if a given service ID is present. Can include matching an individual "
            "service ID, or checking against a list of service IDs.")
        .Example(
            "df = df[px.has_service_id(df.ctx[\"service_id\"], "
            "\"c5f103ab-349e-49e4-8162-3b74f2c07693\")]")
        .Arg("service_id", "The service ID to check")
        .Arg("value", "The value to check for in service")
        .Returns("True if value is present in service_id, otherwise false");
  }
};

// Same functionality as HasServiceNameUDF but with a better name. New class to avoid breaking code
// which uses HasServiceNameUDF
class HasValueUDF : public ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, StringValue array_or_value, StringValue value) {
    return EqualsOrArrayContains(array_or_value, value);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Determine if a value is present in an array.")
        .Details(
            "Checks to see if a given value is present in an array. Can include matching an "
            "individual "
            "value, or checking against an array of services.")
        .Example(
            "df = df[px.has_value(df.ctx[\"replica_set\"], "
            "\"kube-system/kube-dns-79c57c8c9b\")]")
        .Arg("array_or_value", "Array or value to check.")
        .Arg("value", "The value to check for in passed in value.")
        .Returns("True if value is present in the input, otherwise false.");
  }
};

class VizierIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);
    return md->vizier_id().str();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the ID of the cluster.")
        .Details("Get the ID of the Vizier.")
        .Example("df.vizier_id = px.vizier_id()")
        .Returns("The vizier ID of the cluster.");
  }
};

class VizierNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);
    return md->vizier_name();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the name of the cluster.")
        .Details("Get the name of the Vizier.")
        .Example("df.vizier_name = px.vizier_name()")
        .Returns("The name of the cluster according to vizier.");
  }
};

class VizierNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);
    return md->vizier_namespace();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the namespace where Vizier is deployed.")
        .Details("Get the Kubernetes namespace in which the Vizier is deployed.")
        .Example("df.vizier_namespace = px.vizier_namespace()")
        .Returns("The Kubernetes namespace in which Vizier is deployed");
  }
};

class CreateUPIDUDF : public udf::ScalarUDF {
 public:
  UInt128Value Exec(FunctionContext* ctx, Int64Value pid, Int64Value pid_start_time) {
    auto md = GetMetadataState(ctx);
    auto upid = md::UPID(md->asid(), pid.val, pid_start_time.val);
    return upid.value();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a pid, start_time pair to a UPID.")
        .Details("This function creates a UPID from it's underlying components.")
        .Example("df.val = px.upid(df.pid, df.pid_start_time)")
        .Arg("arg1", "The pid of the process.")
        .Arg("arg2", "The start_time of the process.")
        .Returns("The UPID.");
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<CreateUPIDUDF>(types::ST_UPID, {})};
  }
};

class CreateUPIDWithASIDUDF : public udf::ScalarUDF {
 public:
  UInt128Value Exec(FunctionContext*, Int64Value asid, Int64Value pid, Int64Value pid_start_time) {
    auto upid = md::UPID(asid.val, pid.val, pid_start_time.val);
    return upid.value();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a pid, start_time pair to a UPID.")
        .Details("This function creates a UPID from it's underlying components.")
        .Example("df.val = px.upid(px.asid(), df.pid, df.pid_start_time)")
        .Arg("arg1", "The asid of the pem where the process is located.")
        .Arg("arg2", "The pid of the process.")
        .Arg("arg3", "The start_time of the process.")
        .Returns("The UPID.");
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<CreateUPIDWithASIDUDF>(types::ST_UPID, {})};
  }
};

class GetClusterCIDRRangeUDF : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);
    std::set<std::string> cidr_strs;

    auto pod_cidrs = md->k8s_metadata_state().pod_cidrs();
    for (const auto& cidr : pod_cidrs) {
      cidr_strs.insert(cidr.ToString());

      if (cidr.ip_addr.family == px::InetAddrFamily::kIPv4) {
        // Add a CIDR specifically for the CNI bridge, since pod_cidrs don't include that.
        px::CIDRBlock bridge_cidr;
        bridge_cidr.ip_addr = cidr.ip_addr;
        bridge_cidr.prefix_length = 32;
        auto& ipv4_addr = std::get<struct in_addr>(bridge_cidr.ip_addr.addr);
        // Replace the lowest byte with 1, since s_addr is in big endian, we replace the first
        // byte.
        ipv4_addr.s_addr = (ipv4_addr.s_addr & 0x00ffffff) | 0x01000000;

        cidr_strs.insert(bridge_cidr.ToString());
      }
      // TODO(james): add support for IPv6 CNI bridge.
    }
    auto service_cidr = md->k8s_metadata_state().service_cidr();
    if (service_cidr.has_value()) {
      cidr_strs.insert(service_cidr.value().ToString());
    }
    std::vector<std::string> cidr_vec(cidr_strs.begin(), cidr_strs.end());
    cidrs_str_ = VectorToStringArray(cidr_vec);
    return Status::OK();
  }

  StringValue Exec(FunctionContext*) { return cidrs_str_; }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the pod/service CIDRs for the cluster.")
        .Details(
            "Get a json-encoded array of pod/service CIDRs for the cluster in 'ip/prefix_length' "
            "format. Including CIDRs that include the CNI bridge for pods.")
        .Example("df.cidrs = px.get_cidrs()")
        .Returns("The pod and/or service CIDRs for this cluster, encoded as a json array.");
  }

 private:
  std::string cidrs_str_;
};

class NamespaceNameToNamespaceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue namespace_name) {
    auto md = GetMetadataState(ctx);
    auto namespace_id =
        md->k8s_metadata_state().NamespaceIDByName(std::make_pair(namespace_name, namespace_name));
    return namespace_id;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the Kubernetes UID of the given namespace name.")
        .Details("Get the Kubernetes UID of the given namespace name.")
        .Example("df.kube_system_namespace_uid = px.namespace_name_to_namespace_id('kube-system')")
        .Arg("arg1", "The name of the namespace to get the UID of.")
        .Returns("The Kubernetes UID of the given namespace name");
  }
};

void RegisterMetadataOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace px
