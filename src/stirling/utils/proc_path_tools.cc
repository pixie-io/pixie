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

#include "src/stirling/utils/proc_path_tools.h"

#include <filesystem>
#include <memory>
#include <utility>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"

using ::px::system::ProcParser;

namespace px {
namespace stirling {

namespace {

/**
 * Maps the given mount_point within a process to a mount point within the host.
 * These mount points can be different if the process is a container.
 *
 * @param mount_infos Mount info for the process, which may be in a namespace.
 * @param root_mount_infos Mount info for the host.
 * @param mount_point The mount point to resolve.
 * @return The mount point resolved to the host.
 */
StatusOr<std::filesystem::path> ResolveMountPointImpl(const MountInfoVec& mount_infos,
                                                      const MountInfoVec& root_mount_infos,
                                                      const std::filesystem::path& mount_point) {
  std::string device_number;
  std::string device_root;
  for (const auto& mount_info : mount_infos) {
    if (mount_info.mount_point == mount_point.string()) {
      device_number = mount_info.dev;
      device_root = mount_info.root;
      break;
    }
  }
  if (device_number.empty() || device_root.empty()) {
    return error::InvalidArgument("Mount info does not have the requested mount_point=$0",
                                  mount_point.string());
  }

  // Now looks for the mount point of any of the devices' filesystem that can be the parent of
  // the device root path of the requested mount point.
  //
  // For example, assuming the input pid has a MountInfo as follows:
  // {0:1, /foo/bar, /tmp}
  //
  // To look for an accessible mount point for '/tmp', we look for mount points of any one of
  // {/, /foo, /foo/bar} on device 0:1's filesystem. Assuming pid 1 has a MountInfo like this:
  // {0:1, /, /tmp}
  //
  // We can access device 0:1's root through /tmp, and should return /tmp/foo/bar, through which
  // the input pid's '/tmp' can be accessed.
  for (const auto& mount_info : root_mount_infos) {
    if (mount_info.dev != device_number) {
      continue;
    }
    auto rel_path_or = fs::GetChildRelPath(device_root, mount_info.root);
    if (!rel_path_or.ok()) {
      continue;
    }
    const std::filesystem::path device_mount_point(mount_info.mount_point);
    return fs::JoinPath({&device_mount_point, &rel_path_or.ValueOrDie()});
  }
  return error::InvalidArgument("Could not find mount point for to device=$0 root=$1",
                                device_number, device_root);
}

}  // namespace

StatusOr<std::unique_ptr<FilePathResolver>> FilePathResolver::Create(pid_t pid) {
  auto fp_resolver = std::unique_ptr<FilePathResolver>(new FilePathResolver());

  // Populate root mount infos.
  PL_RETURN_IF_ERROR(fp_resolver->Init());

  // Populate mount infos for requested PID.
  PL_RETURN_IF_ERROR(fp_resolver->SetMountNamespace(pid));

  return fp_resolver;
}

Status FilePathResolver::Init() {
  constexpr int kRootPID = 1;

  system::ProcParser proc_parser(system::Config::GetInstance());

  // In case Init() gets called as part of re-initialization in the future,
  // make sure we start with a clean slate.
  pid_mount_infos_.clear();

  // Create an entry for the root PID.
  auto mount_infos = std::make_unique<MountInfoVec>();
  root_mount_infos_ = mount_infos.get();
  pid_mount_infos_[kRootPID] = std::move(mount_infos);

  PL_RETURN_IF_ERROR(proc_parser.ReadMountInfos(kRootPID, root_mount_infos_));

  // Initial state is that we are pointing to the root PID info.
  pid_ = kRootPID;
  mount_infos_ = root_mount_infos_;

  return Status::OK();
}

Status FilePathResolver::SetMountNamespace(pid_t pid) {
  system::ProcParser proc_parser(system::Config::GetInstance());

  if (pid_ == pid) {
    return Status::OK();
  }

  auto [iter, inserted] = pid_mount_infos_.try_emplace(pid);
  if (inserted) {
    // Only populate the mount infos if we don't already have a cached copy.
    iter->second = std::make_unique<MountInfoVec>();
    PL_RETURN_IF_ERROR(proc_parser.ReadMountInfos(pid, iter->second.get()));
  }

  pid_ = pid;
  mount_infos_ = iter->second.get();
  return Status::OK();
}

StatusOr<std::filesystem::path> FilePathResolver::ResolveMountPoint(
    const std::filesystem::path& mount_point) {
  return ResolveMountPointImpl(*mount_infos_, *root_mount_infos_, mount_point);
}

StatusOr<std::filesystem::path> FilePathResolver::ResolvePath(const std::filesystem::path& path) {
  // Find the longest parent path that is accessible of the file, by resolving mount
  // point starting from the immediate parent through the root.
  for (const fs::PathSplit& path_split : fs::EnumerateParentPaths(path)) {
    auto resolved_mount_path_or = ResolveMountPoint(path_split.parent);
    if (resolved_mount_path_or.ok()) {
      return fs::JoinPath({&resolved_mount_path_or.ValueOrDie(), &path_split.child});
    }
  }

  return error::Internal("Could not resolve $0", path.string());
}

StatusOr<std::filesystem::path> GetSelfPath() {
  const system::Config& sysconfig = system::Config::GetInstance();
  ::px::system::ProcParser proc_parser(sysconfig);
  PL_ASSIGN_OR_RETURN(std::filesystem::path self_path, proc_parser.GetExePath(getpid()));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<FilePathResolver> fp_resolver,
                      FilePathResolver::Create(getpid()));
  PL_ASSIGN_OR_RETURN(self_path, fp_resolver->ResolvePath(self_path));
  return sysconfig.ToHostPath(self_path);
}

}  // namespace stirling
}  // namespace px
