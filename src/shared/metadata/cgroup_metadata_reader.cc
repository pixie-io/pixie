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

#include "src/common/base/base.h"
#include "src/common/base/file.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"

namespace px {
namespace md {

CGroupMetadataReader::CGroupMetadataReader(const system::Config& cfg)
    : CGroupMetadataReader(cfg.sysfs_path().string()) {}

CGroupMetadataReader::CGroupMetadataReader(std::string sysfs_path) {
  // Create the new path resolver.
  auto path_resolver_or_status = CGroupPathResolver::Create(sysfs_path);
  path_resolver_ = path_resolver_or_status.ConsumeValueOr(nullptr);

  if (path_resolver_or_status.ok()) {
    LOG(INFO) << absl::Substitute("Using path_resolver with configuration: $0",
                                  path_resolver_->SpecString());
    return;
  }

  // Fallback: Legacy path resolver.
  LOG(ERROR) << absl::Substitute(
      "Failed to create path resolver. Falling back to legacy path resolver. [error = $0]",
      path_resolver_or_status.ToString());

  auto legacy_path_resolver_or_status = LegacyCGroupPathResolver::Create(sysfs_path);
  legacy_path_resolver_ = legacy_path_resolver_or_status.ConsumeValueOr(nullptr);

  if (!legacy_path_resolver_or_status.ok()) {
    LOG(ERROR) << absl::Substitute(
        "Failed to create legacy path resolver. This is not recoverable. [error = $0]",
        legacy_path_resolver_or_status.ToString());
  }
}

StatusOr<std::string> CGroupMetadataReader::PodPath(PodQOSClass qos_class, std::string_view pod_id,
                                                    std::string_view container_id,
                                                    ContainerType container_type) const {
  if (path_resolver_ != nullptr) {
    return path_resolver_->PodPath(qos_class, pod_id, container_id);
  }

  if (legacy_path_resolver_ != nullptr) {
    return legacy_path_resolver_->PodPath(qos_class, pod_id, container_id, container_type);
  }

  return error::Internal("No valid cgroup path resolver.");
}

Status CGroupMetadataReader::ReadPIDs(PodQOSClass qos_class, std::string_view pod_id,
                                      std::string_view container_id, ContainerType container_type,
                                      absl::flat_hash_set<uint32_t>* pid_set) const {
  CHECK(pid_set != nullptr);

  // The container files need to be recursively read and the PIDs needs be merge across all
  // containers.

  PX_ASSIGN_OR_RETURN(std::string fpath, PodPath(qos_class, pod_id, container_id, container_type));

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
