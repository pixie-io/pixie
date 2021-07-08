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
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/file.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"

namespace px {
namespace md {

// There are many different cgroup naming formats used by k8s.
// The standard version is more verbose, and uses underscores instead of dashes.
//
// This is a sample used by GKE:
// /sys/fs/cgroup/cpu,cpuacct/kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/
//        14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs
//
// This is a sample used by a standard kubernetes deployment:
// /sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/
//        docker-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs
//
// This is a sample used by an OpenShift deployment:
// /sys/fs/cgroup/cpu,cpuacct/kubepods.slice/kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2.slice/
//        crio-14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d.scope/cgroup.procs
//
// This is a sample from a bare metal cluster with containerd and k8s 1.21:
// /sys/fs/cgroup/cpu,cpuacct/system.slice/containerd.service/kubepods-besteffort-pod1544eb37_e4f7_49eb_8cc4_3d01c41be77b.slice:cri-containerd:8618d3540ce713dd59ed0549719643a71dd482c40c21685773e7ac1291b004f5/cgroup.procs

CGroupPathResolver::CGroupPathResolver(std::string_view sysfs_path) {
  // Note that as we create these templates, we often substitute in unresolved parameters:
  //  $0 = pod ID
  //  $1 = container ID
  //  $2 = container runtime
  // These template parameters are resolved by calls to PodPath.

  // Different hosts may mount different cgroup dirs. Try a couple for robustness.
  const std::vector<std::string> cgroup_dirs = {"cpu,cpuacct", "cpu", "pids"};

  for (const auto& cgroup_dir : cgroup_dirs) {
    // Attempt assuming naming scheme #1.
    std::string cgroup_kubepods_base_path =
        absl::Substitute("$0/cgroup/$1/kubepods", sysfs_path, cgroup_dir);
    if (fs::Exists(cgroup_kubepods_base_path).ok()) {
      cgroup_kubepod_guaranteed_path_template_ =
          absl::StrCat(cgroup_kubepods_base_path, "/pod$0/$1/cgroup.procs");
      cgroup_kubepod_besteffort_path_template_ =
          absl::StrCat(cgroup_kubepods_base_path, "/besteffort/pod$0/$1/cgroup.procs");
      cgroup_kubepod_burstable_path_template_ =
          absl::StrCat(cgroup_kubepods_base_path, "/burstable/pod$0/$1/cgroup.procs");
      cgroup_kubepod_convert_dashes_ = false;
      return;
    }

    // Attempt assuming naming scheme #3.
    // Must be before the scheme below, since there have been systems that have both paths,
    // but this must take priority.
    cgroup_kubepods_base_path =
        absl::Substitute("$0/cgroup/$1/system.slice/containerd.service", sysfs_path, cgroup_dir);
    if (fs::Exists(cgroup_kubepods_base_path).ok()) {
      cgroup_kubepod_guaranteed_path_template_ =
          absl::StrCat(cgroup_kubepods_base_path, "/kubepods-pod$0.slice:$2:$1/cgroup.procs");
      cgroup_kubepod_besteffort_path_template_ = absl::StrCat(
          cgroup_kubepods_base_path, "/kubepods-besteffort-pod$0.slice:$2:$1/cgroup.procs");
      cgroup_kubepod_burstable_path_template_ = absl::StrCat(
          cgroup_kubepods_base_path, "/kubepods-burstable-pod$0.slice:$2:$1/cgroup.procs");
      cgroup_kubepod_convert_dashes_ = true;
      return;
    }

    // Attempt assuming naming scheme #2.
    cgroup_kubepods_base_path =
        absl::Substitute("$0/cgroup/$1/kubepods.slice", sysfs_path, cgroup_dir);
    if (fs::Exists(cgroup_kubepods_base_path).ok()) {
      cgroup_kubepod_guaranteed_path_template_ =
          absl::StrCat(cgroup_kubepods_base_path, "/kubepods-pod$0.slice/$2-$1.scope/cgroup.procs");
      cgroup_kubepod_besteffort_path_template_ = absl::StrCat(
          cgroup_kubepods_base_path,
          "/kubepods-besteffort.slice/kubepods-besteffort-pod$0.slice/$2-$1.scope/cgroup.procs");
      cgroup_kubepod_burstable_path_template_ = absl::StrCat(
          cgroup_kubepods_base_path,
          "/kubepods-burstable.slice/kubepods-burstable-pod$0.slice/$2-$1.scope/cgroup.procs");
      cgroup_kubepod_convert_dashes_ = true;
      return;
    }
  }

  LOG(ERROR) << absl::Substitute("Could not find kubepods slice under sysfs ($0)", sysfs_path);
}

namespace {
std::string_view ToString(ContainerType container_type) {
  switch (container_type) {
    case ContainerType::kCRIO:
      return "crio";
    case ContainerType::kDocker:
      return "docker";
    case ContainerType::kContainerd:
      return "cri-containerd";
    default:
      // By default, assume any unknown container type is a docker image, to account
      // for older ContainerUpdates which may not have a type.
      return "docker";
  }
}
}  // namespace

std::string CGroupPathResolver::PodPath(PodQOSClass qos_class, std::string_view pod_id,
                                        std::string_view container_id,
                                        ContainerType container_type) const {
  std::string_view path_template;
  switch (qos_class) {
    case PodQOSClass::kGuaranteed:
      path_template = cgroup_kubepod_guaranteed_path_template_;
      break;
    case PodQOSClass::kBestEffort:
      path_template = cgroup_kubepod_besteffort_path_template_;
      break;
    case PodQOSClass::kBurstable:
      path_template = cgroup_kubepod_burstable_path_template_;
      break;
    default:
      LOG(DFATAL) << "Unknown QOS class";
  }

  // Convert any dashes to underscores, because there are two conventions.
  std::string formatted_pod_id(pod_id);
  if (cgroup_kubepod_convert_dashes_) {
    std::replace(formatted_pod_id.begin(), formatted_pod_id.end(), '-', '_');
  }

  return absl::Substitute(path_template, formatted_pod_id, container_id, ToString(container_type));
}

CGroupMetadataReader::CGroupMetadataReader(const system::Config& cfg)
    : path_resolver_(cfg.sysfs_path().string()) {}

Status CGroupMetadataReader::ReadPIDs(PodQOSClass qos_class, std::string_view pod_id,
                                      std::string_view container_id, ContainerType container_type,
                                      absl::flat_hash_set<uint32_t>* pid_set) const {
  CHECK(pid_set != nullptr);

  // The container files need to be recursively read and the PID needs be merge across all
  // containers.

  auto fpath = path_resolver_.PodPath(qos_class, pod_id, container_id, container_type);
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
