#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/metadata/pids.h"

namespace pl {
namespace md {

using K8sMetadataObjectUPtr = std::unique_ptr<K8sMetadataObject>;
using ContainerInfoUPtr = std::unique_ptr<ContainerInfo>;
using PIDInfoUPtr = std::unique_ptr<PIDInfo>;
using AgentID = sole::uuid;

/**
 * This class contains all kubernetes relate metadata.
 */
class K8sMetadataState : NotCopyable {
 public:
  using PodUpdate = pl::shared::k8s::metadatapb::PodUpdate;
  using ContainerUpdate = pl::shared::k8s::metadatapb::ContainerUpdate;
  using ServiceUpdate = pl::shared::k8s::metadatapb::ServiceUpdate;
  using NamespaceUpdate = pl::shared::k8s::metadatapb::NamespaceUpdate;

  // K8s names consist of both a namespace and name : <ns, name>.
  using K8sNameIdent = std::pair<std::string, std::string>;
  using K8sNameIdentView = std::pair<std::string_view, std::string_view>;

  /**
   * K8sIdentHashEq provides hash an ident functions to allow
   * heterogeneous lookups of maps.
   */
  struct K8sIdentHashEq {
    /**
     * K8sIdentCheck validates that the template argument is one of the valid ident types.
     */
    template <typename T>
    static constexpr void K8sIdentCheck() {
      static_assert(
          std::is_base_of<K8sNameIdent, T>::value || std::is_base_of<K8sNameIdentView, T>::value,
          "T must be a K8s ident");
    }

    struct Hash {
      using is_transparent = void;

      template <typename T>
      size_t operator()(const T& v) const {
        K8sIdentCheck<T>();
        return absl::Hash<K8sNameIdentView>{}(v);
      }
    };

    struct Eq {
      using is_transparent = void;

      template <typename T1, typename T2>
      size_t operator()(const T1& a, const T2& b) const {
        K8sIdentCheck<T1>();
        K8sIdentCheck<T2>();
        return a.first == b.first && a.second == b.second;
      }
    };
  };
  using PodsByNameMap =
      absl::flat_hash_map<K8sNameIdent, UID, K8sIdentHashEq::Hash, K8sIdentHashEq::Eq>;

  using ServicesByNameMap = PodsByNameMap;
  using NamespacesByNameMap = PodsByNameMap;
  using PodsByPodIpMap = absl::flat_hash_map<std::string, UID>;

  void set_service_cidr(CIDRBlock cidr) {
    if (!service_cidr_.has_value() || service_cidr_.value() != cidr) {
      LOG(INFO) << absl::Substitute("Service CIDR updated to $0", ToString(cidr));
    }
    service_cidr_ = cidr;
  }
  const std::optional<CIDRBlock>& service_cidr() const { return service_cidr_; }

  void set_pod_cidrs(std::vector<CIDRBlock> cidrs) { pod_cidrs_ = std::move(cidrs); }

  const std::vector<CIDRBlock>& pod_cidrs() const { return pod_cidrs_; }

  const PodsByNameMap& pods_by_name() const { return pods_by_name_; }

  /**
   * PodInfoByID gets an unowned pointer to the Pod. This pointer will remain active
   * for the lifetime of this metadata state instance.
   * @param pod_id the id of the POD.
   * @return Pointer to the PodInfo.
   */
  const PodInfo* PodInfoByID(UIDView pod_id) const;

  /**
   * PodIDByName returns the PodID for the pod of the given name.
   * @param pod_name the pod name
   * @return the pod id or empty string if the pod does not exist.
   */
  UID PodIDByName(K8sNameIdentView pod_name) const;

  /**
   * PodIDByIP returns the PodID for the pod with the given IP.
   * @param pod_ip string of the pod ip.
   * @return the pod_id or empty string if the pod does not exist.
   */
  UID PodIDByIP(std::string_view pod_ip) const;

  /**
   * ContainerInfoByID returns the container info by ID.
   * @param id The ID of the container.
   * @return ContainerInfo or nullptr if not found.
   */
  const ContainerInfo* ContainerInfoByID(CIDView id) const;

  const ServicesByNameMap& services_by_name() const { return services_by_name_; }

  const NamespacesByNameMap& namespaces_by_name() const { return namespaces_by_name_; }

  /**
   * ServiceInfoByID gets an unowned pointer to the Service. This pointer will remain active
   * for the lifetime of this metadata state instance.
   * @param service_id the id of the Service.
   * @return Pointer to the ServiceInfo.
   */
  const ServiceInfo* ServiceInfoByID(UIDView service_id) const;

  /**
   * ServiceIDByName returns the ServiceID for the service of the given name.
   * @param service_name the service name
   * @return the service id or empty string if the service does not exist.
   */
  UID ServiceIDByName(K8sNameIdentView service_name) const;

  /**
   * NamespaceInfoByID gets an unowned pointer to the Namespace. This pointer will remain active
   * for the lifetime of this metadata state instance.
   * @param ns_id the id of the Namespace.
   * @return Pointer to the NamespaceInfo.
   */
  const NamespaceInfo* NamespaceInfoByID(UIDView ns_id) const;

  /**
   * NamespaceIDByName returns the NamespaceID for the namespace of the given name.
   * @param namespace_name the namespace name
   * @return the namespace id or empty string if the namespace does not exist.
   */
  UID NamespaceIDByName(K8sNameIdentView namespace_name) const;

  std::unique_ptr<K8sMetadataState> Clone() const;

  Status HandlePodUpdate(const PodUpdate& update);
  Status HandleContainerUpdate(const ContainerUpdate& update);
  Status HandleServiceUpdate(const ServiceUpdate& update);
  Status HandleNamespaceUpdate(const NamespaceUpdate& update);

  absl::flat_hash_map<CID, ContainerInfoUPtr>& containers_by_id() { return containers_by_id_; }
  std::string DebugString(int indent_level = 0) const;

 private:
  // The CIDR block used for services inside the cluster.
  std::optional<CIDRBlock> service_cidr_;

  // The CIDRs used for pods inside the cluster.
  std::vector<CIDRBlock> pod_cidrs_;

  // This stores K8s native objects (services, pods, etc).
  absl::flat_hash_map<UID, K8sMetadataObjectUPtr> k8s_objects_;

  // TODO(zasgar/michelle): Add heterogeneous lookup from std::pair<string_view, string_view>.
  /**
   * Mapping of pods by name.
   */
  PodsByNameMap pods_by_name_;

  /**
   * Mapping of services by name.
   */
  ServicesByNameMap services_by_name_;

  /**
   * Mapping of namespaces by name.
   */
  NamespacesByNameMap namespaces_by_name_;

  /**
   * Mapping of Pods by host ip.
   */
  PodsByPodIpMap pods_by_ip_;

  /**
   * Mapping of containers by ID.
   */
  absl::flat_hash_map<CID, ContainerInfoUPtr> containers_by_id_;
};

class AgentMetadataState : NotCopyable {
 public:
  AgentMetadataState() = delete;
  explicit AgentMetadataState(uint32_t asid)
      : AgentMetadataState(/* hostname */ "unknown", asid, sole::uuid()) {}

  AgentMetadataState(std::string_view hostname, uint32_t asid, AgentID agent_id)
      : hostname_(std::string(hostname)),
        asid_(asid),
        agent_id_(agent_id),
        k8s_metadata_state_(new K8sMetadataState()) {}

  const std::string& hostname() const { return hostname_; }
  uint32_t asid() const { return asid_; }
  const sole::uuid& agent_id() const { return agent_id_; }

  int64_t last_update_ts_ns() const { return last_update_ts_ns_; }
  void set_last_update_ts_ns(int64_t last_update_ts_ns) { last_update_ts_ns_ = last_update_ts_ns; }

  uint64_t epoch_id() const { return epoch_id_; }
  void set_epoch_id(uint64_t id) { epoch_id_ = id; }

  // Returns an un-owned pointer to the underlying k8s state.
  K8sMetadataState* k8s_metadata_state() { return k8s_metadata_state_.get(); }
  const K8sMetadataState& k8s_metadata_state() const { return *k8s_metadata_state_; }

  std::shared_ptr<AgentMetadataState> CloneToShared() const;

  PIDInfo* GetPIDByUPID(UPID upid) const {
    auto it = pids_by_upid_.find(upid);
    if (it != pids_by_upid_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void AddUPID(UPID upid, std::unique_ptr<PIDInfo> pid_info) {
    pids_by_upid_[upid] = std::move(pid_info);
  }

  void MarkUPIDAsStopped(UPID upid, int64_t ts) {
    auto* pid_info = GetPIDByUPID(upid);
    if (pid_info != nullptr) {
      pid_info->set_stop_time_ns(ts);
    }
  }

  const absl::flat_hash_map<UPID, PIDInfoUPtr>& pids_by_upid() const { return pids_by_upid_; }

  absl::flat_hash_set<md::UPID> upids() const {
    absl::flat_hash_set<md::UPID> upids;
    for (const auto& [upid, pid_info] : pids_by_upid()) {
      if (pid_info == nullptr || pid_info->stop_time_ns() > 0) {
        // PID has been stopped.
        continue;
      }
      upids.insert(upid);
    }
    return upids;
  }

  std::string DebugString(int indent_level = 0) const;

 private:
  /**
   * Tracks the time that this K8s metadata object was created. The object should be periodically
   * refreshed to get the latest version.
   */
  int64_t last_update_ts_ns_ = 0;

  /**
   * A monotonically increasing number that tracks the epoch of this K8s state.
   * The epoch is incremented everytime a new MetadataState is created. MetadataState objects after
   * creation should be immutable to allow concurrent read access without locks.
   */
  uint64_t epoch_id_ = 0;

  std::string hostname_;
  uint32_t asid_;
  AgentID agent_id_;

  std::unique_ptr<K8sMetadataState> k8s_metadata_state_;

  /**
   * Mapping of PIDs by UPID for active pods on the system.
   */
  absl::flat_hash_map<UPID, PIDInfoUPtr> pids_by_upid_;
};

}  // namespace md
}  // namespace pl
