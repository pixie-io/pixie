#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"

namespace pl {
namespace md {

/*************************************************
 * Constants for the /proc/<pid>/stat file
 *************************************************/
constexpr int kProcStatNumFields = 52;
constexpr int kProcStatPIDField = 0;
constexpr int kProcStatStartTimeField = 21;

/**
 * Constants controlling where in sys/fs to look for the cgroups data.
 */
constexpr char kSysfsCpuAcctPatch[] = "cgroup/cpu,cpuacct/kubepods";
constexpr std::string_view kPidFile = "cgroup.procs";

std::string CGroupMetadataReader::CGroupProcFilePath(std::string_view sysfs_prefix,
                                                     PodQOSClass qos_class, std::string_view pod_id,
                                                     std::string_view container_id) {
  switch (qos_class) {
    case PodQOSClass::kGuaranteed:
      return absl::Substitute("$0/$1/pod$2/$3/$4", sysfs_prefix, kSysfsCpuAcctPatch, pod_id,
                              container_id, kPidFile);
    case PodQOSClass::kBestEffort:
      return absl::Substitute("$0/$1/besteffort/pod$2/$3/$4", sysfs_prefix, kSysfsCpuAcctPatch,
                              pod_id, container_id, kPidFile);
    case PodQOSClass::kBurstable:
      return absl::Substitute("$0/$1/burstable/pod$2/$3/$4", sysfs_prefix, kSysfsCpuAcctPatch,
                              pod_id, container_id, kPidFile);
    default:
      CHECK(0) << "Unknown QOS class";
  }
}

// TODO(zasgar/michelle): Reconcile this code with cgroup manager. We should delete the cgroup
// manager version of the code after the transition to the new metadata scheme is complete.
Status CGroupMetadataReader::ReadPIDs(PodQOSClass qos_class, std::string_view pod_id,
                                      std::string_view container_id,
                                      absl::flat_hash_set<uint32_t>* pid_set) const {
  CHECK(pid_set != nullptr);

  // The container files need to be recursively read and the PID needs be merge across all
  // containers.

  auto fpath = CGroupProcFilePath(sysfs_path_, qos_class, pod_id, container_id);
  std::ifstream ifs(fpath);
  if (!ifs) {
    // This might not be a real error since the pod could have disappeared.
    LOG(WARNING) << absl::Substitute("Failed to open file $0", fpath);
    return error::Unknown("Failed to open file $0", fpath);
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
Status CGroupMetadataReader::ReadPIDMetadata(uint32_t pid, PIDMetadata* out) const {
  CHECK(out != nullptr);
  PL_RETURN_IF_ERROR(ReadPIDStatFile(pid, out));
  return ReadPIDCmdLineFile(pid, out);
}

Status CGroupMetadataReader::ReadPIDCmdLineFile(uint32_t pid, PIDMetadata* out) const {
  CHECK(out != nullptr);

  std::string fpath = absl::Substitute("$0/$1/cmdline", proc_path_, pid);
  std::ifstream ifs(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
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

  out->cmdline_args = std::move(cmdline);

  return Status::OK();
}

Status CGroupMetadataReader::ReadPIDStatFile(uint32_t pid, PIDMetadata* out) const {
  CHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/$1/stat", proc_path_, pid);
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
  }

  std::string line;
  bool ok = true;
  if (!std::getline(ifs, line)) {
    return error::Internal("Failed to read file $0", fpath);
  }

  std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
  // We check less than in case more fields are added later.
  if (split.size() < kProcStatNumFields) {
    return error::Unknown("Incorrect number of fields in stat file: $0", fpath);
  }
  ok &= absl::SimpleAtoi(split[kProcStatPIDField], &out->pid);
  ok &= absl::SimpleAtoi(split[kProcStatStartTimeField], &out->start_time_ns);

  out->start_time_ns *= ns_per_kernel_tick_;
  out->start_time_ns += clock_realtime_offset_;

  if (!ok) {
    // This should never happen since it requires the file to be ill-formed
    // by the kernel.
    return error::Internal("failed to parse stat file ($0). ATOI failed.", fpath);
  }

  return Status::OK();
}

}  // namespace md
}  // namespace pl
