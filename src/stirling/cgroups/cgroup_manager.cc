#include <experimental/filesystem>
#include <fstream>
#include <istream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "src/common/base/base.h"
#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;

constexpr char kSysfsCpuAcctPatch[] = "cgroup/cpu,cpuacct/kubepods";
constexpr std::string_view kPodPrefix = "pod";
constexpr std::string_view kPidFile = "cgroup.procs";

namespace {

Status ReadPIDList(const fs::path& pid_file_path, std::vector<int64_t>* pid_list) {
  CHECK(pid_list != nullptr);
  std::ifstream ifs(pid_file_path);
  if (!ifs.good()) {
    return error::Unknown("Failed to open file: $0", pid_file_path.string());
  }
  std::string line;
  while (std::getline(ifs, line)) {
    if (line.empty()) {
      continue;
    }
    int64_t pid;
    if (!absl::SimpleAtoi(line, &pid)) {
      return error::Unknown("Failed to parse pid file");
    }
    pid_list->emplace_back(pid);
  }
  return Status::OK();
}

}  // namespace

std::unique_ptr<CGroupManager> CGroupManager::Create(std::string_view proc_path,
                                                     std::string_view sysfs_path) {
  auto syscfg = common::SystemConfig::GetInstance();
  if (!syscfg->HasSystemConfig()) {
    LOG(ERROR) << "CGroupManager requires SystemConfig";
    return nullptr;
  }
  return CGroupManager::Create(*syscfg, proc_path, sysfs_path);
}

std::unique_ptr<CGroupManager> CGroupManager::Create(const common::SystemConfig& cfg,
                                                     std::string_view proc_path,
                                                     std::string_view sysfs_path) {
  std::unique_ptr<CGroupManager> retval(new CGroupManager(cfg, proc_path, sysfs_path));
  return retval;
}

Status CGroupManager::UpdateQoSClassInfo(fs::path qos_path, CGroupQoS qos) {
  std::error_code ec;

  // It is not an error if the directory doesn't exist.
  // There might be no pods in that QoS class.
  if (!fs::exists(qos_path)) {
    return Status::OK();
  }

  auto dir_iter = fs::directory_iterator(qos_path, ec);
  if (ec) {
    return error::Unknown("Failed to open: $0: $1", qos_path.string(), ec.message());
  }

  for (const auto& p : dir_iter) {
    auto path_str = p.path().string();
    auto pod_name = p.path().filename().string();
    if (fs::is_directory(p.path()) && absl::StartsWith(pod_name, kPodPrefix)) {
      // Update cgroup_info_ with pod and container details.
      PL_RETURN_IF_ERROR(UpdatePodInfo(p.path(), pod_name, qos));
    }
  }
  return Status::OK();
}

Status CGroupManager::UpdatePodInfo(fs::path pod_path, const std::string& pod_name, CGroupQoS qos) {
  std::error_code ec;
  auto pod_dir_iter = fs::directory_iterator(pod_path, ec);
  if (ec) {
    return error::Unknown("Failed to open: $0: $1", pod_path.string(), ec.message());
  }

  PodInfo pod_info;
  pod_info.qos = qos;
  for (const auto& container_path : pod_dir_iter) {
    if (fs::is_directory(container_path.path())) {
      PL_RETURN_IF_ERROR(UpdateContainerInfo(container_path.path(), &pod_info));
    }
  }
  cgroup_info_[pod_name] = std::move(pod_info);

  return Status::OK();
}

Status CGroupManager::UpdateContainerInfo(const fs::path& container_path, PodInfo* pod_info) {
  std::error_code ec;
  bool is_directory = fs::is_directory(container_path, ec);

  if (ec) {
    return error::Unknown("Failed to open: $0: $1", container_path.string(), ec.message());
  }
  if (!is_directory) {
    return error::Unknown("Container path is not a directory: $0", container_path.string());
  }

  auto container_name = container_path.filename().string();
  ContainerInfo info;
  // Read the pid list.
  PL_RETURN_IF_ERROR(ReadPIDList(container_path / kPidFile, &info.pids));
  pod_info->container_info_by_name[container_name] = std::move(info);
  return Status::OK();
}

Status CGroupManager::UpdateCGroupInfo() {
  // On cgroup update we need to rescan the entire filesystem and update
  // the internal data structures.
  cgroup_info_.clear();

  // Sysfs base path must be valid.
  bool is_directory = fs::is_directory(sysfs_path_);
  if (!is_directory) {
    return error::Unknown("Sysfs path is not a directory: $0", sysfs_path_.string());
  }

  auto base_path = sysfs_path_ / kSysfsCpuAcctPatch;

  // K8s has three different QoS classes and with the exception of the
  // guaranteed class they are placed in sub directories.
  fs::path guaranteed_path = base_path;
  fs::path burstable_path = base_path / "burstable";
  fs::path best_effort_path = base_path / "besteffort";

  PL_RETURN_IF_ERROR(UpdateQoSClassInfo(guaranteed_path, CGroupQoS::kGuaranteed));
  PL_RETURN_IF_ERROR(UpdateQoSClassInfo(burstable_path, CGroupQoS::kBurstable));
  PL_RETURN_IF_ERROR(UpdateQoSClassInfo(best_effort_path, CGroupQoS::kBestEffort));

  full_scan_count_++;

  return Status::OK();
}

Status CGroupManager::GetNetworkStatsForPod(const std::string& pod,
                                            ProcParser::NetworkStats* stats) {
  DCHECK(stats != nullptr);
  PL_ASSIGN_OR_RETURN(const auto* cgroup_info, GetCGroupInfoForPod(pod));
  for (const auto& container_info : cgroup_info->container_info_by_name) {
    for (const int64_t pid : container_info.second.pids) {
      auto s = proc_parser_.ParseProcPIDNetDev(proc_parser_.GetProcPidNetDevFile(pid), stats);
      // Since all the containers running in a K8s pod use the same network
      // namespace we only, need to pull stats from a single PID. The stats
      // themselves are the same for each PID since Linux only tracks networks
      // stats at a namespace level.
      //
      // In case the read fails we try another file. This should not normally
      // be required, but will make the code more robust to cases where the PID
      // is killed between when we update the pid list but before the network
      // data is requested.
      if (s.ok()) {
        return Status::OK();
      }
      LOG(ERROR) << absl::Substitute("Failed to read stats for pid: $0, trying next", pid);
    }
  }

  return error::Unknown("failed to read network stats.");
}

Status CGroupManager::GetProcessStats(int64_t pid, ProcParser::ProcessStats* stats) {
  DCHECK(stats != nullptr);
  auto proc_stat_path = proc_parser_.GetProcPidStatFilePath(pid);
  auto proc_io_path = proc_parser_.GetProcPidStatIOFile(pid);

  PL_RETURN_IF_ERROR(proc_parser_.ParseProcPIDStat(proc_stat_path, stats));
  PL_RETURN_IF_ERROR(proc_parser_.ParseProcPIDStatIO(proc_io_path, stats));

  return Status::OK();
}

StatusOr<const CGroupManager::PodInfo*> CGroupManager::GetCGroupInfoForPod(const std::string& pod) {
  const auto& cgroup_info = cgroup_info_.find(pod);
  if (cgroup_info == end(cgroup_info_)) {
    return error::NotFound("Pod $0 not found, while fetching cgroup info", pod);
  }
  return &cgroup_info->second;
}

StatusOr<const std::vector<int64_t>*> CGroupManager::PIDsInContainer(const std::string& pod,
                                                                     const std::string& container) {
  const auto& cgroup_it = cgroup_info_.find(pod);
  if (cgroup_it == end(cgroup_info_)) {
    return error::NotFound("pod not found: $0", pod);
  }
  const auto& container_it = cgroup_it->second.container_info_by_name.find(container);
  if (container_it == end(cgroup_it->second.container_info_by_name)) {
    return error::NotFound("container not found: $0", container);
  }
  return &container_it->second.pids;
}

}  // namespace stirling
}  // namespace pl
