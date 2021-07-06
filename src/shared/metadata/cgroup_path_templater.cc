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

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

#include <absl/strings/str_replace.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/shared/metadata/cgroup_path_templater.h"

namespace px {
namespace md {

StatusOr<std::string> CGroupBasePath(std::string_view sysfs_path) {
  // Different hosts may mount different cgroup dirs. Try a couple for robustness.
  const std::vector<std::string> cgroup_dirs = {"cpu,cpuacct", "cpu", "pids"};

  for (const auto& cgroup_dir : cgroup_dirs) {
    std::filesystem::path dir;

    // Attempt assuming naming scheme #1.
    std::string base_path = absl::StrCat(sysfs_path, "/cgroup/", cgroup_dir);
    if (fs::Exists(base_path).ok()) {
      return base_path;
    }
  }

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
  const std::regex kPodIDRegex(
      R"(pod[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12}\b)");

  // Pattern match for a container ID.
  // Example: 8618d3540ce713dd59ed0549719643a71dd482c40c21685773e7ac1291b004f5
  const std::regex kContainerIDRegex(R"(\b[0-9a-f]{64}\b)");

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
  spec.templated_path = std::regex_replace(spec.templated_path, kPodIDRegex, R"($$0)");
  spec.templated_path = std::regex_replace(spec.templated_path, kContainerIDRegex, R"($$1)");
  spec.templated_path = absl::StrReplaceAll(
      spec.templated_path,
      {{"burstable/", ""}, {"burstable-", ""}, {"besteffort/", ""}, {"besteffort-", ""}});
  spec.templated_path = absl::StrReplaceAll(
      spec.templated_path, {{"kubepods/", "kubepods/$2/"}, {"kubepods-", "kubepods-$2-"}});

  return spec;
}

StatusOr<CGroupTemplateSpec> AutoDiscoverCGroupTemplate(std::string_view sysfs_path) {
  PL_ASSIGN_OR_RETURN(std::string base_path, CGroupBasePath(sysfs_path));
  LOG(INFO) << "Auto-discovered CGroup base path: " << base_path;

  PL_ASSIGN_OR_RETURN(std::string self_cgroup_procs, FindSelfCGroupProcs(base_path));
  LOG(INFO) << "Auto-discovered example path: " << self_cgroup_procs;

  PL_ASSIGN_OR_RETURN(CGroupTemplateSpec cgroup_path_template,
                      CreateCGroupTemplateSpecFromPath(self_cgroup_procs));
  LOG(INFO) << "Auto-discovered template: " << cgroup_path_template.templated_path;

  return cgroup_path_template;
}

std::string CGroupTemplater::Evaluate(PodQOSClass qos_class, std::string_view pod_id,
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
    path = absl::StrReplaceAll(path, {{"--", "-"}, {"//", "/"}});
  }

  return path;
}

}  // namespace md
}  // namespace px
