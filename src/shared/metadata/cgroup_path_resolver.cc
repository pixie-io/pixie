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
#include <linux/magic.h>
#include <sys/vfs.h>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

#include <absl/strings/str_replace.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/shared/metadata/cgroup_path_resolver.h"

DEFINE_bool(force_cgroup2_mode, true, "Flag to force assume cgroup2 fs for testing purposes");

namespace px {
namespace md {

StatusOr<std::string> CGroupBasePath(std::string_view sysfs_path) {
  // Different hosts may mount different cgroup dirs. Try a couple for robustness.
  const std::vector<std::string> cgroup_dirs = {"cpu,cpuacct", "cpu", "pids"};

  for (const auto& cgroup_dir : cgroup_dirs) {
    std::filesystem::path dir;

    // Attempt assuming naming scheme #1.
    std::string base_path = absl::StrCat(sysfs_path, "/cgroup/", cgroup_dir);

    if (fs::Exists(base_path)) {
      return base_path;
    }
  }

  std::string cgv2_base_path = absl::StrCat(sysfs_path, "/cgroup");
  struct statfs info;
  auto fs_status = statfs(cgv2_base_path.c_str(), &info);
  bool cgroupv2 = (fs_status == 0) && (info.f_type == CGROUP2_SUPER_MAGIC);

  if (cgroupv2 || FLAGS_force_cgroup2_mode) {
    return cgv2_base_path;
  }
  // (TODO): This check for cgroup2FS is eventually to be moved above the cgroupv1 check.

  return error::NotFound("Could not find CGroup base path");
}

StatusOr<std::string> FindSelfCGroupProcs(std::string_view base_path) {
  int pid = getpid();

  for (auto& p : std::filesystem::recursive_directory_iterator(base_path)) {
    if (p.path().filename() == "cgroup.procs") {
      std::string contents = ReadFileToString(p.path().string()).ValueOr("");
      int contents_pid;
      if (absl::SimpleAtoi(contents, &contents_pid) && pid == contents_pid) {
        return p.path().string();
      }
    }
  }

  return error::NotFound("Could not find self as a template.");
}

StatusOr<CGroupTemplateSpec> CreateCGroupTemplateSpecFromPath(std::string_view path) {
  // Pattern match for a pod ID.
  // Examples:
  //   pod8dbc5577_d0e2_4706_8787_57d52c03ddf2
  //   pod8dbc5577-d0e2-4706-8787-57d52c03ddf2
  static std::regex kPodIDRegex(
      R"(pod[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12}\b)");

  // Pattern match for a container ID.
  // Example: 8618d3540ce713dd59ed0549719643a71dd482c40c21685773e7ac1291b004f5
  static std::regex kContainerIDRegex(R"(\b[0-9a-f]{64}\b)");

  CGroupTemplateSpec spec;

  // Detect which format the path uses:
  //   kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2
  //   kubepods/pod8dbc5577_d0e2_4706_8787_57d52c03ddf2
  if (absl::StrContains(path, "kubepods-")) {
    spec.qos_separator = '-';
  } else if (absl::StrContains(path, "kubepods/")) {
    spec.qos_separator = '/';
  } else {
    return error::NotFound("Unexpected cgroup path format [example path: $0].", path);
  }

  // Detect whether the pod IDs uses dashes or underscores.:
  //   kubepods-pod8dbc5577_d0e2_4706_8787_57d52c03ddf2
  //   kubepods/pod8dbc5577_d0e2_4706_8787_57d52c03ddf2
  std::smatch pod_id_match;
  std::string path_str(path);
  bool found = std::regex_search(path_str, pod_id_match, kPodIDRegex);
  if (!found) {
    return error::NotFound("Unexpected cgroup path format [example path: $0].", path);
  }
  std::string pod_id_string = pod_id_match.str();

  if (absl::StrContains(pod_id_string, "-")) {
    spec.pod_id_separators = '-';
  } else if (absl::StrContains(pod_id_string, "_")) {
    spec.pod_id_separators = '_';
  } else {
    spec.pod_id_separators = std::nullopt;
  }

  // Create templated path.
  //  - Replace pod ID with $0
  //  - Replace container ID with $1
  //  - Replace qos with $2
  spec.templated_path = std::string(path);
  spec.templated_path = std::regex_replace(spec.templated_path, kPodIDRegex, R"(pod$$0)");
  spec.templated_path = std::regex_replace(spec.templated_path, kContainerIDRegex, R"($$1)");
  spec.templated_path =
      absl::StrReplaceAll(spec.templated_path, {{"burstable", "$2"}, {"besteffort", "$2"}});
  spec.templated_path = absl::StrReplaceAll(
      spec.templated_path,
      {{"kubepods/pod", "kubepods/$2/pod"}, {"kubepods-pod", "kubepods-$2-pod"}});

  return spec;
}

StatusOr<CGroupTemplateSpec> AutoDiscoverCGroupTemplate(std::string_view sysfs_path) {
  PX_ASSIGN_OR_RETURN(std::string base_path, CGroupBasePath(sysfs_path));
  LOG(INFO) << "Auto-discovered CGroup base path: " << base_path;

  PX_ASSIGN_OR_RETURN(std::string self_cgroup_procs, FindSelfCGroupProcs(base_path));
  LOG(INFO) << "Auto-discovered example path: " << self_cgroup_procs;

  PX_ASSIGN_OR_RETURN(CGroupTemplateSpec cgroup_path_template,
                      CreateCGroupTemplateSpecFromPath(self_cgroup_procs));
  LOG(INFO) << "Auto-discovered template: " << cgroup_path_template.templated_path;

  return cgroup_path_template;
}

StatusOr<std::unique_ptr<CGroupPathResolver>> CGroupPathResolver::Create(
    std::string_view sysfs_path) {
  PX_ASSIGN_OR_RETURN(CGroupTemplateSpec spec, AutoDiscoverCGroupTemplate(sysfs_path));
  return std::unique_ptr<CGroupPathResolver>(new CGroupPathResolver(spec));
}

std::string CGroupPathResolver::PodPath(PodQOSClass qos_class, std::string_view pod_id,
                                        std::string_view container_id) {
  // Convert any dashes to underscores, because there are two conventions.
  std::string formatted_pod_id(pod_id);
  if (spec_.pod_id_separators.has_value()) {
    formatted_pod_id = absl::StrReplaceAll(
        formatted_pod_id, {{"-", std::string(1, spec_.pod_id_separators.value())}});
  }

  std::string qos_str;
  switch (qos_class) {
    case PodQOSClass::kBestEffort:
      qos_str = "besteffort";
      break;
    case PodQOSClass::kBurstable:
      qos_str = "burstable";
      break;
    case PodQOSClass::kGuaranteed:
      qos_str = "";
      break;
    default:
      LOG(ERROR) << "Unexpected PodQOSClass";
  }

  std::string path =
      absl::Substitute(spec_.templated_path, formatted_pod_id, container_id, qos_str);

  if (qos_class == PodQOSClass::kGuaranteed) {
    path = absl::StrReplaceAll(path, {{"--", "-"}, {"/kubepods-.slice/", "/"}, {"//", "/"}});
  }

  return path;
}

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

StatusOr<std::unique_ptr<LegacyCGroupPathResolver>> LegacyCGroupPathResolver::Create(
    std::string_view sysfs_path) {
  auto resolver = std::unique_ptr<LegacyCGroupPathResolver>(new LegacyCGroupPathResolver);
  PX_RETURN_IF_ERROR(resolver->Init(sysfs_path));
  return resolver;
}

Status LegacyCGroupPathResolver::Init(std::string_view sysfs_path) {
  // Note that as we create these templates, we often substitute in unresolved parameters:
  //  $0 = pod ID
  //  $1 = container ID
  //  $2 = container runtime
  // These template parameters are resolved by calls to PodPath.
  // Different hosts may mount different cgroup dirs. Try a couple for robustness.
  PX_ASSIGN_OR_RETURN(std::string cgroup_dir, CGroupBasePath(sysfs_path));

  // Attempt assuming naming scheme #1.
  std::string cgroup_kubepods_base_path = absl::Substitute("$0/kubepods", cgroup_dir);
  if (fs::Exists(cgroup_kubepods_base_path)) {
    cgroup_kubepod_guaranteed_path_template_ =
        absl::StrCat(cgroup_kubepods_base_path, "/pod$0/$1/cgroup.procs");
    cgroup_kubepod_besteffort_path_template_ =
        absl::StrCat(cgroup_kubepods_base_path, "/besteffort/pod$0/$1/cgroup.procs");
    cgroup_kubepod_burstable_path_template_ =
        absl::StrCat(cgroup_kubepods_base_path, "/burstable/pod$0/$1/cgroup.procs");
    cgroup_kubepod_convert_dashes_ = false;
    return Status::OK();
  }

  // Attempt assuming naming scheme #3.
  // Must be before the scheme below, since there have been systems that have both paths,
  // but this must take priority.
  cgroup_kubepods_base_path = absl::Substitute("$0/system.slice/containerd.service", cgroup_dir);
  if (fs::Exists(cgroup_kubepods_base_path)) {
    cgroup_kubepod_guaranteed_path_template_ =
        absl::StrCat(cgroup_kubepods_base_path, "/kubepods-pod$0.slice:$2:$1/cgroup.procs");
    cgroup_kubepod_besteffort_path_template_ = absl::StrCat(
        cgroup_kubepods_base_path, "/kubepods-besteffort-pod$0.slice:$2:$1/cgroup.procs");
    cgroup_kubepod_burstable_path_template_ = absl::StrCat(
        cgroup_kubepods_base_path, "/kubepods-burstable-pod$0.slice:$2:$1/cgroup.procs");
    cgroup_kubepod_convert_dashes_ = true;
    return Status::OK();
  }

  // Attempt assuming naming scheme #2.
  cgroup_kubepods_base_path = absl::Substitute("$0/kubepods.slice", cgroup_dir);
  if (fs::Exists(cgroup_kubepods_base_path)) {
    cgroup_kubepod_guaranteed_path_template_ =
        absl::StrCat(cgroup_kubepods_base_path, "/kubepods-pod$0.slice/$2-$1.scope/cgroup.procs");
    cgroup_kubepod_besteffort_path_template_ = absl::StrCat(
        cgroup_kubepods_base_path,
        "/kubepods-besteffort.slice/kubepods-besteffort-pod$0.slice/$2-$1.scope/cgroup.procs");
    cgroup_kubepod_burstable_path_template_ = absl::StrCat(
        cgroup_kubepods_base_path,
        "/kubepods-burstable.slice/kubepods-burstable-pod$0.slice/$2-$1.scope/cgroup.procs");
    cgroup_kubepod_convert_dashes_ = true;
    return Status::OK();
  }

  return error::NotFound("Could not find kubepods slice under sysfs ($0)", sysfs_path);
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

std::string LegacyCGroupPathResolver::PodPath(PodQOSClass qos_class, std::string_view pod_id,
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

}  // namespace md
}  // namespace px
