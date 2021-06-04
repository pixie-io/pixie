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
#include <string>
#include <utility>
#include <vector>

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

class PodNameToPodIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    return GetPodID(md, pod_name);
  }

  static StringValue GetPodID(const px::md::AgentMetadataState* md, StringValue pod_name) {
    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PL_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
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
    PL_ASSIGN_OR(auto k8s_name_view, internal::K8sName(pod_name), return "");
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

inline types::StringValue StringifyVector(const std::vector<std::string>& vec) {
  if (vec.size() == 1) {
    return std::string(vec[0]);
  } else if (vec.size() > 1) {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartArray();
    for (const auto& str : vec) {
      writer.String(str.c_str());
    }
    writer.EndArray();
    return s.GetString();
  }
  return "";
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
    return udf::ScalarUDFDocBuilder("Convert the kubernetes service ID to service name.")
        .Details(
            "Converts the kubernetes service ID to the name of the service. If the ID "
            "is not found in our mapping, then returns an empty string.")
        .Example("df.service = px.service_id_to_service_name(df.service_id)")
        .Arg("service_id", "The service ID to get the service name for.")
        .Returns("The service name or an empty string if service_id not found.");
  }
};

class ServiceNameToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue service_name) {
    auto md = GetMetadataState(ctx);
    // This UDF expects the service name to be in the format of "<ns>/<service-name>".
    PL_ASSIGN_OR(auto service_name_view, internal::K8sName(service_name), return "");
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
    PL_ASSIGN_OR(auto service_name_view, internal::K8sName(service_name), return "");
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
 * @brief Returns the service names for the given pod name.
 */
class PodNameToServiceNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    PL_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
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
    PL_ASSIGN_OR(auto pod_name_view, internal::K8sName(pod_name), return "");
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
                      (ready_status->second == md::PodConditionStatus::kTrue);
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
        .Example("df.pod_status = px.pod_name_to_pod_status(df.pod_name)")
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
    return ready_status->second == md::PodConditionStatus::kTrue;
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

class PodIPToPodIDUDF : public ScalarUDF {
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

class PodIPToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_ip) {
    auto md = GetMetadataState(ctx);
    auto pod_id = md->k8s_metadata_state().PodIDByIP(pod_ip);
    if (pod_id == "") {
      return "";
    }
    PodIDToServiceIDUDF udf;
    return udf.Exec(ctx, pod_id);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the service ID for a given pod IP.")
        .Details(
            "Converts the IP address into a Kubernetes service ID for the service "
            "associated to the pod. If there is no service associated with the given IP address, "
            "return an empty string.")
        .Example("df.service_id = px.ip_to_service_id(df.remote_addr)")
        .Arg("pod_ip", "The IP of a pod to convert.")
        .Returns("The service id if it exists, otherwise an empty string.");
  }
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

void RegisterMetadataOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace px
