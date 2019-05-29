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
#include "src/common/fs/fs.h"
#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;

constexpr char kSysfsCpuAcctPatch[] = "cgroup/cpu,cpuacct/kubepods";
constexpr std::string_view kPodPrefix = "pod";
constexpr std::string_view kPidFile = "cgroup.procs";

namespace {

Status ReadPIDList(fs::path pid_file_path, std::vector<int64_t>* pid_list) {
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
  auto syscfg = common::SystemConfig::Create();
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

void CGroupManager::AddFSWatch(const fs::path& path) {
  if (!fs_watcher_) {
    return;
  }
  auto s = fs_watcher_->AddWatch(path);
  if (!s.ok()) {
    // Cannot rely on fs watcher anymore. Reset the unique pointer.
    fs_watcher_.reset();
    LOG(INFO) << absl::StrFormat("Could not add watcher for path: %s, error: %s", path.string(),
                                 s.msg());
  }
}

void CGroupManager::RemoveFSWatch(const fs::path& path) {
  if (!fs_watcher_) {
    return;
  }
  auto s = fs_watcher_->RemoveWatch(path);
  if (!s.ok()) {
    // Cannot rely on fs watcher anymore. Reset the unique pointer.
    fs_watcher_.reset();
    LOG(INFO) << absl::StrFormat("Could not remove watcher for path: %s, error: %s", path.string(),
                                 s.msg());
  }
}

Status CGroupManager::UpdatePodInfo(fs::path pod_path, const std::string& pod_name, CGroupQoS qos) {
  std::error_code ec;
  auto container_dir_iter = fs::directory_iterator(pod_path, ec);
  if (ec) {
    return error::Unknown("Failed to open: $0: $1", pod_path.string(), ec.message());
  }

  AddFSWatch(pod_path);
  PodInfo pod_info;
  pod_info.qos = qos;
  for (const auto& container_path : container_dir_iter) {
    PL_RETURN_IF_ERROR(UpdateContainerInfo(container_path.path(), &pod_info));
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
  AddFSWatch(container_path / kPidFile);
  pod_info->container_info_by_name[container_name] = std::move(info);
  return Status::OK();
}

Status CGroupManager::UpdateCGroupInfoForQoSClass(CGroupQoS qos, fs::path base_path) {
  std::error_code ec;
  auto dir_iter = fs::directory_iterator(base_path, ec);
  if (ec) {
    return error::Unknown("Failed to open: $0: $1", base_path.string(), ec.message());
  }

  AddFSWatch(base_path);
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

Status CGroupManager::HandleFSPodEvent(const fs::path& path, FSWatcher::FSEventType event_type,
                                       const std::string& pod_name) {
  if (event_type == FSWatcher::FSEventType::kDeleteDir) {
    cgroup_info_.erase(pod_name);
    RemoveFSWatch(path / pod_name);
    return Status::OK();
  }

  // TODO(kgandhi): Since we know the exact path, we should use that to
  // determine the qos instead of StrContains.
  CGroupQoS qos = CGroupQoS::kGuaranteed;
  if (absl::StrContains(path.string(), "besteffort")) {
    qos = CGroupQoS::kBestEffort;
  } else if (absl::StrContains(path.string(), "burstable")) {
    qos = CGroupQoS::kBurstable;
  }
  return UpdatePodInfo(path, pod_name, qos);
}

Status CGroupManager::HandleFSContainerEvent(const fs::path& path,
                                             FSWatcher::FSEventType event_type,
                                             const std::string& container_name) {
  auto pod_name = path.filename().string();
  auto pod_info = cgroup_info_[pod_name];
  if (event_type == FSWatcher::FSEventType::kDeleteDir) {
    pod_info.container_info_by_name.erase(container_name);
    RemoveFSWatch(path / container_name / kPidFile);
    return Status::OK();
  }

  return UpdateContainerInfo(path / container_name, &pod_info);
}

Status CGroupManager::HandleFSEvent(FSWatcher::FSEvent* fs_event) {
  auto path = fs_event->GetPath();
  switch (fs_event->type) {
    case FSWatcher::FSEventType::kCreateDir:
      // fall through
    case FSWatcher::FSEventType::kDeleteDir: {
      auto dir_name = fs_event->name;
      bool is_pod = absl::StartsWith(fs_event->name, kPodPrefix);
      if (is_pod) {
        return HandleFSPodEvent(path, fs_event->type, dir_name);
      }
      return HandleFSContainerEvent(path, fs_event->type, dir_name);
    }
    case FSWatcher::FSEventType::kModifyFile: {
      auto container_path = path.parent_path();
      auto pod_name = container_path.parent_path().filename().string();
      return UpdateContainerInfo(container_path, &(cgroup_info_[pod_name]));
    }
    case FSWatcher::FSEventType::kUnknown:
      // fall through
    default:
      return error::Unknown("Unknown FS watcher event type.");
  }
  return Status::OK();
}

Status CGroupManager::ScanFileSystem() {
  cgroup_info_.clear();
  auto base_path = fs::path(sysfs_path_) / kSysfsCpuAcctPatch;
  // K8s has three different QoS classes and with the exception of the
  // guaranteed class they are placed i sub directories.
  fs::path best_effort_path = base_path / "besteffort";
  fs::path burstable_path = base_path / "burstable";
  fs::path guaranteed_path = base_path;

  PL_RETURN_IF_ERROR(UpdateCGroupInfoForQoSClass(CGroupQoS::kBestEffort, best_effort_path));
  PL_RETURN_IF_ERROR(UpdateCGroupInfoForQoSClass(CGroupQoS::kBurstable, burstable_path));
  PL_RETURN_IF_ERROR(UpdateCGroupInfoForQoSClass(CGroupQoS::kGuaranteed, guaranteed_path));

  return Status::OK();
}

Status CGroupManager::UpdateCGroupInfo() {
  // No system support for inotify. Always scan file system.
  if (!FSWatcher::SupportsInotify()) {
    return ScanFileSystem();
  }

  // There was potentially an error while adding or removing watchers
  // in a previous iteration or needs to be created for the first time.
  if (!fs_watcher_) {
    fs_watcher_ = FSWatcher::Create();
    return ScanFileSystem();
  }

  // Scan the filesystem: FS Watcher not initialized
  if (fs_watcher_->NotInitialized()) {
    return ScanFileSystem();
  }

  if (fs_watcher_->HasOverflow()) {
    // To avoid race conditions on an exisiting inotify fd, destroy existing
    // fs_watcher_ and recreate it.
    fs_watcher_ = FSWatcher::Create();
    return ScanFileSystem();
  }

  // Handle fs watcher events.
  if (!fs_watcher_->ReadInotifyUpdates().ok()) {
    LOG(INFO) << "Could not read inotfy updated from FS Watcher.";
    return ScanFileSystem();
  }
  while (fs_watcher_->HasEvents()) {
    auto event_status = fs_watcher_->GetNextEvent();
    if (!event_status.ok()) {
      LOG(INFO) << "Could not get next event from FS Watcher.";
      return ScanFileSystem();
    }
    auto fs_event = event_status.ConsumeValueOrDie();
    if (fs_event.type == FSWatcher::FSEventType::kUnknown) {
      LOG(INFO) << "FS Watcher reported an unknown event.";
      return ScanFileSystem();
    }
    auto handle_status = HandleFSEvent(&fs_event);

    if (!handle_status.ok()) {
      LOG(INFO) << "Could not handle FS Watcher event: " << handle_status.msg();
      return ScanFileSystem();
    }
  }
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
      // namespace we only, need to pull stats from a single PID. The stas
      // themselves are the same for each PID since Linux only tracks networks
      // stats at a namespace level.
      //
      // In case the read fails we try another file. This should not noramally
      // be required, but will make the code more robust to cases where the PID
      // is killed between when we update the pid list but before the network
      // data is requested.
      if (s.ok()) {
        return Status::OK();
      }
      LOG(ERROR) << absl::StrFormat("Failed to read stats for pid: %ld, trying next", pid);
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
