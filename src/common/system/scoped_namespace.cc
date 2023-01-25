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

#include "src/common/system/scoped_namespace.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "src/common/system/config.h"
#include "src/common/system/proc_pid_path.h"

namespace px {
namespace system {

StatusOr<std::unique_ptr<ScopedNamespace>> ScopedNamespace::Create(int ns_pid,
                                                                   std::string_view ns_type) {
  auto scoped_namespace = std::unique_ptr<ScopedNamespace>(new ScopedNamespace);
  PX_RETURN_IF_ERROR(scoped_namespace->EnterNamespace(ns_pid, ns_type));
  return scoped_namespace;
}

ScopedNamespace::~ScopedNamespace() { ExitNamespace(); }

Status ScopedNamespace::EnterNamespace(int ns_pid, std::string_view ns_type) {
  const std::filesystem::path orig_ns_path = ProcPath("self", "ns", ns_type);
  orig_ns_fd_ = open(orig_ns_path.string().c_str(), O_RDONLY);
  if (orig_ns_fd_ < 0) {
    return error::Internal("Could not access original namespace FD [path=$0]",
                           orig_ns_path.string());
  }

  std::filesystem::path ns_path = ProcPidPath(ns_pid, "ns", ns_type);
  ns_fd_ = open(ns_path.string().c_str(), O_RDONLY);
  if (ns_fd_ < 0) {
    return error::Internal("Could not access target namespace FD [path=$0]", ns_path.string());
  }

  // Switch network namespaces, so socket prober connects to the target network namespace.
  setns_retval_ = setns(ns_fd_, 0);
  if (setns_retval_ != 0) {
    return error::Internal("Could not change to $0 namespace of PID $1 [err=$2]", ns_type, ns_pid,
                           std::strerror(errno));
  }

  return Status::OK();
}

void ScopedNamespace::ExitNamespace() {
  // Process in reverse order of EnterNamespace.
  if (setns_retval_ != 0) {
    return;
  }
  ECHECK_EQ(setns(orig_ns_fd_, 0), 0) << "Uh-oh...could not restore original namespace.";

  if (ns_fd_ < 0) {
    return;
  }
  close(ns_fd_);

  if (orig_ns_fd_ < 0) {
    return;
  }
  close(orig_ns_fd_);
}

}  // namespace system
}  // namespace px
