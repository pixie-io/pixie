#include <experimental/filesystem>
#include <fstream>
#include <istream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "src/common/base/base.h"
#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;

constexpr char kSysfsCpuAcctPatch[] = "cgroup/cpu,cpuacct/kubepods";
constexpr std::string_view kPodPrefix = "pod";
constexpr std::string_view kPidFile = "cgroup.procs";

namespace {

Status ReadPIDList(fs::path pid_file_path, std::vector<int64_t> *pid_list) {
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
  int64_t ns_per_jiffy = 0;
  int bytes_per_page = 0;
#ifdef __linux__
  ns_per_jiffy = static_cast<int64_t>(1E9 / sysconf(_SC_CLK_TCK));
  bytes_per_page = sysconf(_SC_PAGESIZE);
#else
  LOG(FATAL) << "Not supported on OS other than linux";
#endif
  return CGroupManager::Create(proc_path, sysfs_path, bytes_per_page, ns_per_jiffy);
}

std::unique_ptr<CGroupManager> CGroupManager::Create(std::string_view proc_path,
                                                     std::string_view sysfs_path,
                                                     int bytes_per_page, int64_t ns_per_jiffy) {
  std::unique_ptr<CGroupManager> retval(
      new CGroupManager(proc_path, sysfs_path, bytes_per_page, ns_per_jiffy));
  return retval;
}

Status CGroupManager::UpdateCGroupInfoForQoSClass(CGroupQoS qos, fs::path base_path) {
  std::error_code ec;
  auto dir_iter = fs::directory_iterator(base_path, ec);
  if (ec) {
    return error::Unknown("Failed to open: $0: $1", base_path.string(), ec.message());
  }

  for (const auto &p : dir_iter) {
    auto path_str = p.path().string();
    if (std::experimental::filesystem::is_directory(p.path()) &&
        absl::StartsWith(p.path().filename().string(), kPodPrefix)) {
      auto pod_name = p.path().filename().string();

      PodInfo cgroup;
      cgroup.qos = qos;

      auto container_dir_iter = fs::directory_iterator(p.path(), ec);
      if (ec) {
        return error::Unknown("Failed to open: $0: $1", p.path().string(), ec.message());
      }
      for (const auto &container_path : container_dir_iter) {
        if (std::experimental::filesystem::is_directory(container_path.path())) {
          auto container_name = container_path.path().filename().string();
          ContainerInfo info;
          // Read the pid list.
          PL_RETURN_IF_ERROR(ReadPIDList(container_path.path() / kPidFile, &info.pids));
          cgroup.container_info_by_name[container_name] = info;
        }
      }
      cgroup_info_[pod_name] = cgroup;
    }
  }

  return Status::OK();
}

Status CGroupManager::UpdateCGroupInfo() {
  auto base_path = fs::path(sysfs_path_) / kSysfsCpuAcctPatch;
  // TODO(zasgar/kgandhi): This is really inefficient, we should use inotify or something
  // to capture the changes.
  cgroup_info_.clear();

  // K8s has three different QoS classes and with the exception of the guaranteed class they are
  // placed i sub directories.
  fs::path best_effort_path = base_path / "besteffort";
  fs::path burstable_path = base_path / "burstable";
  fs::path guaranteed_path = base_path;

  PL_RETURN_IF_ERROR(UpdateCGroupInfoForQoSClass(CGroupQoS::kBestEffort, best_effort_path));
  PL_RETURN_IF_ERROR(UpdateCGroupInfoForQoSClass(CGroupQoS::kBurstable, burstable_path));
  PL_RETURN_IF_ERROR(UpdateCGroupInfoForQoSClass(CGroupQoS::kGuaranteed, guaranteed_path));

  return Status::OK();
}

Status CGroupManager::GetNetworkStatsForPod(const std::string &pod,
                                            proc_parser::NetworkStats *stats) {
  DCHECK(stats != nullptr);
  PL_ASSIGN_OR_RETURN(const auto *cgroup_info, GetCGroupInfoForPod(pod));
  for (const auto &container_info : cgroup_info->container_info_by_name) {
    for (const int64_t pid : container_info.second.pids) {
      auto s = proc_parser::ParseProcPIDNetDev(proc_parser::GetProcPidNetDevFile(pid, proc_path_),
                                               stats);
      // Since all the containers running in a K8s pod use the same network namespace we only,
      // need to pull stats from a single PID. The stas themselves are the same for each PID since
      // Linux only tracks networks stats at a namespace level.
      //
      // In case the read fails we try another file. This should not noramally be required, but
      // will make the code more robust to cases where the PID is killed between when we update
      // the pid list but before the network data is requested.
      if (s.ok()) {
        return Status::OK();
      }
      LOG(ERROR) << absl::StrFormat("Failed to read stats for pid: %ld, trying next", pid);
    }
  }

  return error::Unknown("failed to read network stats.");
}

Status CGroupManager::GetProcessStats(int64_t pid, proc_parser::ProcessStats *stats) {
  DCHECK(stats != nullptr);
  auto proc_stat_path = proc_parser::GetProcPidStatFilePath(pid, proc_path_);
  auto proc_io_path = proc_parser::GetProcPidStatIOFile(pid, proc_path_);

  PL_RETURN_IF_ERROR(
      proc_parser::ParseProcPIDStat(proc_stat_path, stats, ns_per_jiffy_, bytes_per_page_));
  PL_RETURN_IF_ERROR(proc_parser::ParseProcPIDStatIO(proc_io_path, stats));

  return Status::OK();
}

StatusOr<const CGroupManager::PodInfo *> CGroupManager::GetCGroupInfoForPod(
    const std::string &pod) {
  const auto &cgroup_info = cgroup_info_.find(pod);
  if (cgroup_info == end(cgroup_info_)) {
    return error::NotFound("Pod $0 not found, while fetching cgroup info", pod);
  }
  return &cgroup_info->second;
}

StatusOr<const std::vector<int64_t> *> CGroupManager::PIDsInContainer(
    const std::string &pod, const std::string &container) {
  const auto &cgroup_it = cgroup_info_.find(pod);
  if (cgroup_it == end(cgroup_info_)) {
    return error::NotFound("pod not found: $0", pod);
  }
  const auto &container_it = cgroup_it->second.container_info_by_name.find(container);
  if (container_it == end(cgroup_it->second.container_info_by_name)) {
    return error::NotFound("container not found: $0", container);
  }
  return &container_it->second.pids;
}

}  // namespace stirling
}  // namespace pl
