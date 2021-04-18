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

#pragma once

#include <memory>

#include "src/common/base/statusor.h"

namespace px {
namespace system {

/**
 * ScopedNamespace is a scoped utility to enter a Linux namespace.
 * It automatically restores the namespace once the scope is terminated.
 */
class ScopedNamespace {
 public:
  /**
   * Create a scoped namespace defined by pid and type.
   * The namespace will be restored to its original value when the scope exits.
   *
   * @param ns_pid Target namespace, specified by pid.
   * @param ns_type Type of namespace (e.g. pid, net, mnt, ...)
   *                Consult `ls /proc/<pid>/ns` for other values.
   * @return error if the namespace was not entered.
   */
  static StatusOr<std::unique_ptr<ScopedNamespace>> Create(int ns_pid, std::string_view ns_type);

  /**
   * Cleanup FDs and restore namespace.
   */
  ~ScopedNamespace();

 private:
  Status EnterNamespace(int ns_pid, std::string_view ns_type);
  void ExitNamespace();

  int orig_ns_fd_ = -1;
  int ns_fd_ = -1;
  int setns_retval_ = -1;
};

}  // namespace system
}  // namespace px
