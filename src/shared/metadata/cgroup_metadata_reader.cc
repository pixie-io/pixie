#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/file.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"

namespace pl {
namespace md {

/*************************************************
 * Constants for the /proc/<pid>/stat file
 *************************************************/
constexpr int kProcStatNumFields = 52;
constexpr int kProcStatStartTimeField = 21;

// Note that there are different cgroup naming formats used by Kuberenetes under sysfs.
// The standard version is more verbose, and uses underscores instead of dashes.
//
// This is a sample used by GKE:
// /sys/fs/cgroup/cpu,cpuacct/kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/
//        14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
//
// This is a sample used by a standard kubernetes deployment:
// /sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/
//        docker-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs

void CGroupMetadataReader::InitPathTemplates(std::string_view proc_path,
                                             std::string_view sysfs_path) {
  // Note that as we create these templates, we often substitute in unresolved parameters by using
  // $0. For example, the pod ID is left as a template parameter to be resolved later.

  proc_stat_path_template_ = absl::Substitute("$0/$1/stat", proc_path, "$0");
  proc_cmdline_path_template_ = absl::Substitute("$0/$1/cmdline", proc_path, "$0");

  // Attempt assuming naming scheme #1.
  constexpr char kSysfsCpuAcctPath1[] = "cgroup/cpu,cpuacct/kubepods";
  std::string cgroup_kubepods_base_path = absl::Substitute("$0/$1", sysfs_path, kSysfsCpuAcctPath1);
  if (FileExists(cgroup_kubepods_base_path)) {
    cgroup_kubepod_guaranteed_path_template_ =
        absl::Substitute("$0/pod$1", cgroup_kubepods_base_path, "$0");
    cgroup_kubepod_besteffort_path_template_ =
        absl::Substitute("$0/besteffort/pod$1", cgroup_kubepods_base_path, "$0");
    cgroup_kubepod_burstable_path_template_ =
        absl::Substitute("$0/burstable/pod$1", cgroup_kubepods_base_path, "$0");
    container_template_ = "/$0/$1";
    cgroup_kubepod_convert_dashes_ = false;
    return;
  }

  // Attempt assuming naming scheme #2.
  constexpr char kSysfsCpuAcctPath2[] = "cgroup/cpu,cpuacct/kubepods.slice";
  cgroup_kubepods_base_path = absl::Substitute("$0/$1", sysfs_path, kSysfsCpuAcctPath2);
  if (FileExists(cgroup_kubepods_base_path)) {
    cgroup_kubepod_guaranteed_path_template_ =
        absl::Substitute("$0/kubepods-pod$1.slice", cgroup_kubepods_base_path, "$0");
    cgroup_kubepod_besteffort_path_template_ =
        absl::Substitute("$0/kubepods-besteffort.slice/kubepods-besteffort-pod$1.slice",
                         cgroup_kubepods_base_path, "$0");
    cgroup_kubepod_burstable_path_template_ =
        absl::Substitute("$0/kubepods-burstable.slice/kubepods-burstable-pod$1.slice",
                         cgroup_kubepods_base_path, "$0");
    container_template_ = "/docker-$0.scope/$1";
    cgroup_kubepod_convert_dashes_ = true;
    return;
  }

  LOG(ERROR) << absl::Substitute("Could not find kubepods slice under sysfs ($0)", sysfs_path);
}

CGroupMetadataReader::CGroupMetadataReader(const system::Config& cfg)
    : ns_per_kernel_tick_(static_cast<int64_t>(1E9 / cfg.KernelTicksPerSecond())),
      clock_realtime_offset_(cfg.ClockRealTimeOffset()) {
  InitPathTemplates(cfg.proc_path(), cfg.sysfs_path());
}

std::string CGroupMetadataReader::CGroupPodDirPath(PodQOSClass qos_class,
                                                   std::string_view pod_id) const {
  std::string formatted_pod_id(pod_id);

  // Convert any dashes to underscores, because there are two conventions.
  if (cgroup_kubepod_convert_dashes_) {
    std::replace(formatted_pod_id.begin(), formatted_pod_id.end(), '-', '_');
  }

  switch (qos_class) {
    case PodQOSClass::kGuaranteed:
      return absl::Substitute(cgroup_kubepod_guaranteed_path_template_, formatted_pod_id);
    case PodQOSClass::kBestEffort:
      return absl::Substitute(cgroup_kubepod_besteffort_path_template_, formatted_pod_id);
    case PodQOSClass::kBurstable:
      return absl::Substitute(cgroup_kubepod_burstable_path_template_, formatted_pod_id);
    default:
      CHECK(0) << "Unknown QOS class";
  }
}

std::string CGroupMetadataReader::CGroupProcFilePath(PodQOSClass qos_class, std::string_view pod_id,
                                                     std::string_view container_id) const {
  constexpr std::string_view kPidFile = "cgroup.procs";

  // TODO(oazizi): Might be better to inline code from CGroupPodDirPath for performance reasons
  // (avoid string copies and perform a single absl::Substitute).
  return absl::StrCat(CGroupPodDirPath(qos_class, pod_id),
                      absl::Substitute(container_template_, container_id, kPidFile));
}

// TODO(zasgar/michelle): Reconcile this code with cgroup manager. We should delete the cgroup
// manager version of the code after the transition to the new metadata scheme is complete.
Status CGroupMetadataReader::ReadPIDs(PodQOSClass qos_class, std::string_view pod_id,
                                      std::string_view container_id,
                                      absl::flat_hash_set<uint32_t>* pid_set) const {
  CHECK(pid_set != nullptr);

  // The container files need to be recursively read and the PID needs be merge across all
  // containers.

  auto fpath = CGroupProcFilePath(qos_class, pod_id, container_id);
  std::ifstream ifs(fpath);
  if (!ifs) {
    // This might not be a real error since the pod could have disappeared.
    return error::NotFound("Failed to open file $0", fpath);
  }

  std::string line;
  while (std::getline(ifs, line)) {
    if (line.empty()) {
      continue;
    }
    int64_t pid;
    if (!absl::SimpleAtoi(line, &pid)) {
      LOG(WARNING) << absl::Substitute("Failed to parse pid file: $0", fpath);
      continue;
    }
    pid_set->emplace(pid);
  }
  return Status::OK();
}

// TODO(zasgar/michelle): cleanup and merge with proc_parser.
std::string CGroupMetadataReader::ReadPIDCmdline(uint32_t pid) const {
  std::string fpath = absl::Substitute(proc_cmdline_path_template_, pid);
  std::ifstream ifs(fpath);
  if (!ifs) {
    return "";
  }

  std::string line = "";
  std::string cmdline = "";
  while (std::getline(ifs, line)) {
    cmdline += std::move(line);
  }

  // Strip out extra null character at the end of the string.
  if (!cmdline.empty() && cmdline[cmdline.size() - 1] == 0) {
    cmdline.pop_back();
  }

  // Replace all nulls with spaces. Sometimes the command line has
  // null to separate arguments and others it has spaces. We just make them all spaces
  // and leave it to upstream code to tokenize properly.
  std::replace(cmdline.begin(), cmdline.end(), static_cast<char>(0), ' ');

  return cmdline;
}

int64_t CGroupMetadataReader::ReadPIDStartTimeTicks(uint32_t pid) const {
  std::string fpath = absl::Substitute(proc_stat_path_template_, pid);
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return 0;
  }

  std::string line;
  if (!std::getline(ifs, line)) {
    return 0;
  }

  std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
  // We check less than in case more fields are added later.
  if (split.size() < kProcStatNumFields) {
    return 0;
  }

  int64_t start_time_ticks;
  if (!absl::SimpleAtoi(split[kProcStatStartTimeField], &start_time_ticks)) {
    return 0;
  }

  return start_time_ticks;
}

bool CGroupMetadataReader::PodDirExists(const PodInfo& pod_info) const {
  auto pod_path = CGroupPodDirPath(pod_info.qos_class(), pod_info.uid());
  return FileExists(pod_path);
}

}  // namespace md
}  // namespace pl
