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

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/file.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"

namespace px {
namespace md {

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
// This is a sample used by an OpenShift deployment:
// /sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/
//        crio-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs

void CGroupMetadataReader::InitPathTemplates(std::string_view sysfs_path) {
  // Note that as we create these templates, we often substitute in unresolved parameters by using
  // $0. For example, the pod ID is left as a template parameter to be resolved later.

  // Attempt assuming naming scheme #1.
  constexpr char kSysfsCpuAcctPath1[] = "cgroup/cpu,cpuacct/kubepods";
  std::string cgroup_kubepods_base_path = absl::Substitute("$0/$1", sysfs_path, kSysfsCpuAcctPath1);
  if (fs::Exists(cgroup_kubepods_base_path).ok()) {
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
  if (fs::Exists(cgroup_kubepods_base_path).ok()) {
    cgroup_kubepod_guaranteed_path_template_ =
        absl::Substitute("$0/kubepods-pod$1.slice", cgroup_kubepods_base_path, "$0");
    cgroup_kubepod_besteffort_path_template_ =
        absl::Substitute("$0/kubepods-besteffort.slice/kubepods-besteffort-pod$1.slice",
                         cgroup_kubepods_base_path, "$0");
    cgroup_kubepod_burstable_path_template_ =
        absl::Substitute("$0/kubepods-burstable.slice/kubepods-burstable-pod$1.slice",
                         cgroup_kubepods_base_path, "$0");
    container_template_ = "/$2-$0.scope/$1";
    cgroup_kubepod_convert_dashes_ = true;
    return;
  }

  LOG(ERROR) << absl::Substitute("Could not find kubepods slice under sysfs ($0)", sysfs_path);
}

CGroupMetadataReader::CGroupMetadataReader(const system::Config& cfg)
    : ns_per_kernel_tick_(static_cast<int64_t>(1E9 / cfg.KernelTicksPerSecond())),
      clock_realtime_offset_(cfg.ClockRealTimeOffset()) {
  const std::string sysfs_path_str = cfg.sysfs_path().string();
  InitPathTemplates(sysfs_path_str);
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
                                                     std::string_view container_id,
                                                     ContainerType container_type) const {
  constexpr std::string_view kPidFile = "cgroup.procs";

  // TODO(oazizi): Might be better to inline code from CGroupPodDirPath for performance reasons
  // (avoid string copies and perform a single absl::Substitute).
  std::string containerType;
  switch (container_type) {
    case ContainerType::kCRIO:
      containerType = "crio";
      break;
    case ContainerType::kDocker:
      containerType = "docker";
      break;
    default:
      // By default, assume any unknown container type is a docker image, to account
      // for older ContainerUpdates which may not have a type.
      containerType = "docker";
  }
  return absl::StrCat(CGroupPodDirPath(qos_class, pod_id),
                      absl::Substitute(container_template_, container_id, kPidFile, containerType));
}

Status CGroupMetadataReader::ReadPIDs(PodQOSClass qos_class, std::string_view pod_id,
                                      std::string_view container_id, ContainerType container_type,
                                      absl::flat_hash_set<uint32_t>* pid_set) const {
  CHECK(pid_set != nullptr);

  // The container files need to be recursively read and the PID needs be merge across all
  // containers.

  auto fpath = CGroupProcFilePath(qos_class, pod_id, container_id, container_type);
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

}  // namespace md
}  // namespace px
