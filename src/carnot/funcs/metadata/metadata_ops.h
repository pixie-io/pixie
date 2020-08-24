#pragma once

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

namespace pl {
namespace carnot {
namespace funcs {
namespace metadata {

using ScalarUDF = pl::carnot::udf::ScalarUDF;

namespace internal {
inline rapidjson::GenericStringRef<char> StringRef(std::string_view s) {
  return rapidjson::GenericStringRef<char>(s.data(), s.size());
}
}  // namespace internal

inline const pl::md::AgentMetadataState* GetMetadataState(pl::carnot::udf::FunctionContext* ctx) {
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
};

class UPIDToASIDUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, UInt128Value upid_value) { return upid_value.High64() >> 32; }
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
};

class PodNameToPodIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    return GetPodID(md, pod_name);
  }

  static StringValue GetPodID(const pl::md::AgentMetadataState* md, StringValue pod_name) {
    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    std::vector<std::string_view> name_parts = absl::StrSplit(pod_name, "/");
    if (name_parts.size() != 2) {
      return "";
    }

    auto pod_name_view = std::make_pair(name_parts[0], name_parts[1]);
    auto pod_id = md->k8s_metadata_state().PodIDByName(pod_name_view);

    return pod_id;
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
};

class PodNameToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue pod_name) {
    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    std::vector<std::string_view> name_parts = absl::StrSplit(pod_name, "/");
    if (name_parts.size() != 2) {
      return "";
    }
    return std::string(name_parts[0]);
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<PodNameToNamespaceUDF>(types::ST_NAMESPACE_NAME,
                                                             {types::ST_NONE})};
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
};

inline const md::ContainerInfo* UPIDToContainer(const pl::md::AgentMetadataState* md,
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
};

inline const pl::md::PodInfo* UPIDtoPod(const pl::md::AgentMetadataState* md,
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
};

class ServiceNameToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue service_name) {
    auto md = GetMetadataState(ctx);
    // This UDF expects the service name to be in the format of "<ns>/<service-name>".
    std::vector<std::string_view> name_parts = absl::StrSplit(service_name, "/");
    if (name_parts.size() != 2) {
      return "";
    }

    auto service_name_view = std::make_pair(name_parts[0], name_parts[1]);
    auto service_id = md->k8s_metadata_state().ServiceIDByName(service_name_view);

    return service_id;
  }
};

/**
 * @brief Returns the namespace for a service.
 */
class ServiceNameToNamespaceUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue service_name) {
    // This UDF expects the service name to be in the format of "<ns>/<svc-name>".
    std::vector<std::string_view> name_parts = absl::StrSplit(service_name, "/");
    if (name_parts.size() != 2) {
      return "";
    }
    return std::string(name_parts[0]);
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<ServiceNameToNamespaceUDF>(types::ST_NAMESPACE_NAME,
                                                                 {types::ST_NONE})};
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
};

/**
 * @brief Returns the service names for the given pod name.
 */
class PodNameToServiceNameUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    std::vector<std::string_view> name_parts = absl::StrSplit(pod_name, "/");
    if (name_parts.size() != 2) {
      return "";
    }

    auto pod_name_view = std::make_pair(name_parts[0], name_parts[1]);
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
};

/**
 * @brief Returns the service ids for the given pod name.
 */
class PodNameToServiceIDUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);

    // This UDF expects the pod name to be in the format of "<ns>/<pod-name>".
    std::vector<std::string_view> name_parts = absl::StrSplit(pod_name, "/");
    if (name_parts.size() != 2) {
      return "";
    }

    auto pod_name_view = std::make_pair(name_parts[0], name_parts[1]);
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
};

class UPIDToStringUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, UInt128Value upid_value) {
    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    return upid.String();
  }
};

class UPIDToPIDUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, UInt128Value upid_value) {
    auto upid_uint128 = absl::MakeUint128(upid_value.High64(), upid_value.Low64());
    auto upid = md::UPID(upid_uint128);
    return static_cast<int64_t>(upid.pid());
  }
};

class PodIDToPodStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue pod_id) {
    auto md = GetMetadataState(ctx);
    const pl::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return 0;
    }
    return pod_info->start_time_ns();
  }
};

class PodNameToPodStartTimeUDF : public ScalarUDF {
 public:
  Time64NSValue Exec(FunctionContext* ctx, StringValue pod_name) {
    auto md = GetMetadataState(ctx);
    StringValue pod_id = PodNameToPodIDUDF::GetPodID(md, pod_name);
    const pl::md::PodInfo* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info == nullptr) {
      return 0;
    }
    return pod_info->start_time_ns();
  }
};

inline std::string PodPhaseToString(const pl::md::PodPhase& pod_phase) {
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

inline types::StringValue PodInfoToPodStatus(const pl::md::PodInfo* pod_info) {
  std::string phase = "";
  std::string msg = "";
  std::string reason = "";
  if (pod_info != nullptr) {
    phase = PodPhaseToString(pod_info->phase());
    msg = pod_info->phase_message();
    reason = pod_info->phase_reason();
  }
  rapidjson::Document d;
  d.SetObject();
  d.AddMember("phase", internal::StringRef(phase), d.GetAllocator());
  d.AddMember("message", internal::StringRef(msg), d.GetAllocator());
  d.AddMember("reason", internal::StringRef(reason), d.GetAllocator());
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

inline std::string ContainerStateToString(const pl::md::ContainerState& container_state) {
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

    std::string state = "";
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
        .Details(R"doc(Get the status of the container given the container ID.
            | The status is the K8s state of either 'Running', 'Waiting', 'Terminated' or 'Unknown'.
            | It may be paired with additional information such as a message and reason explaining
            | why the container is in that state.
        )doc")
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
};

inline std::string PodInfoToPodQoS(const pl::md::PodInfo* pod_info) {
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
};

void RegisterMetadataOpsOrDie(pl::carnot::udf::Registry* registry);

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
