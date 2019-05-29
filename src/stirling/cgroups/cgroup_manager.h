#pragma once

#include <experimental/filesystem>

#include <istream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/fs/fs.h"
#include "src/common/system_config/system_config.h"
#include "src/stirling/cgroups/proc_parser.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;

/**
 * CGroupManager keeps track of K8s cgroups and allows fetching of stats for
 * underlying process and network namespaces.
 */
class CGroupManager {
 public:
  /**
   * Create makes a new CGroupManager.
   *
   * This function only works on Linux.
   *
   * @param proc_Path path to the proc files system.
   * @param sysfs_path Path the sys file system.
   * @return unique_ptr to the CGroupManager.
   */
  static std::unique_ptr<CGroupManager> Create(std::string_view proc_path,
                                               std::string_view sysfs_path);

  /**
   * Create makes a new CGroupManager.
   *
   * This version should be used for testing only.
   *
   * @param cfg is a system config reference. Needs to be valid for the duration
   * of the Create call.
   * @param proc_path Path to the proc file system.
   * @param sysfs_path Path to the sysfs file system.
   * @return unique_ptr to the CGroupManager, returns null if it can't construct
   * a valid CGroupManager.
   */
  static std::unique_ptr<CGroupManager> Create(const common::SystemConfig& cfg,
                                               std::string_view proc_path,
                                               std::string_view sysfs_path);

  /**
   * CGroupQoS store the K8S QoS levels.
   */
  enum class CGroupQoS { kUnknown = 0, kBestEffort, kBurstable, kGuaranteed };

  /**
   * CGroupQoSToString converts the CGroupQoS enum to string.
   * @param qos value of the qos.
   * @return string value.
   */
  static std::string CGroupQoSToString(CGroupQoS qos) {
    switch (qos) {
      case CGroupQoS::kBestEffort:
        return "BestEffort";
      case CGroupQoS::kBurstable:
        return "Burstable";
      case CGroupQoS::kGuaranteed:
        return "Guaranteed";
      default:
        return "UNKNOWN";
    }
  }

  // Right now we track information for pods and containers, where containers
  // are sub-entities of a pod. We store them as maps, to deduplicate the names
  // of the containers and pods.
  /**
   * ContainerInfo stores information for a given container.
   */
  struct ContainerInfo {
    std::vector<int64_t> pids;
  };

  /**
   * PodInfo stores information about a specific pod.
   */
  struct PodInfo {
    CGroupQoS qos;
    // The pid for all the containers in this pod.
    std::unordered_map<std::string, ContainerInfo> container_info_by_name;
  };

  /**
   * Rescan the cgroups to update qos/pod/container/pid info.
   * @return Status of the update.
   */
  Status UpdateCGroupInfo();

  /**
   * Get the network stats for a given pod.
   * @param pod The pod name.
   * @param stats The network stats.
   * @return Status of getting the network data.
   */
  Status GetNetworkStatsForPod(const std::string& pod, ProcParser::NetworkStats* stats);

  /**
   * Get the procs stats per pod.
   * @param pid The pid(tgid) of the process.
   * @param stats The stats to be filled in.
   * @return Status of getting process stats.
   */
  Status GetProcessStats(int64_t pid, ProcParser::ProcessStats* stats);

  /**
   * Get the information for a particular pod.
   * @param pod The name of the pod.
   * @return A status or pointer to the pod info. This pointer is valid until
   * UpdateCGroupInfo is called. Note we return a pointer because status or does
   * not like references being returned.
   */
  StatusOr<const PodInfo*> GetCGroupInfoForPod(const std::string& pod);

  /**
   * Get a reference to the underlying cgroup information.
   * This reference is valid until the next time UpdataCGroupInfo is caled.
   *
   * @return reference to cgroup_info.
   */
  const std::unordered_map<std::string, PodInfo>& cgroup_info() { return cgroup_info_; }

  /**
   * HasPod checks if a pod exists.
   * @param pod The name of the pod.
   * @return true if a pod exists.
   */
  bool HasPod(const std::string& pod) { return cgroup_info_.find(pod) != end(cgroup_info_); }

  /**
   * PIDsInContainer returns a list of pids in a given container.
   * @param pod The name of the pod.
   * @param container The name of the container.
   * @return Status or a pointer to the pid list, which is valid until the next
   * time UpdateGroupInfo is called.
   */
  StatusOr<const std::vector<int64_t>*> PIDsInContainer(const std::string& pod,
                                                        const std::string& container);

 protected:
  CGroupManager() = delete;

  CGroupManager(const common::SystemConfig& cfg, std::string_view proc_path,
                std::string_view sysfs_path)
      : proc_parser_(cfg, proc_path), sysfs_path_(sysfs_path) {}

 private:
  /**
   * Rescan the cgroups to update qos/pod/container/pid info.
   * @return Status of the update.
   */
  Status UpdateCGroupInfoForQoSClass(CGroupQoS qos, fs::path base_path);

  Status UpdatePodInfo(fs::path pod_path, const std::string& pod_name, CGroupQoS qos);
  Status UpdateContainerInfo(const fs::path& container_path, PodInfo* pod_info);
  Status HandleFSEvent(FSWatcher::FSEvent* fs_event);
  void AddFSWatch(const fs::path& path);
  void RemoveFSWatch(const fs::path& path);
  Status HandleFSPodEvent(const fs::path& path, FSWatcher::FSEventType event_type,
                          const std::string& pod_name);
  Status HandleFSContainerEvent(const fs::path& path, FSWatcher::FSEventType event_type,
                                const std::string& container_name);

  Status ScanFileSystem();

  ProcParser proc_parser_;
  std::unique_ptr<FSWatcher> fs_watcher_ = nullptr;
  fs::path sysfs_path_;

  // Map from pod name to group info. Pods are unique across QOS classes so we
  // don't need to track that in the key.
  std::unordered_map<std::string, PodInfo> cgroup_info_;
};

}  // namespace stirling
}  // namespace pl
