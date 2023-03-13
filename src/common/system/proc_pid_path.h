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

#include <gflags/gflags.h>
#include <sys/types.h>
#include <filesystem>
#include <string>

DECLARE_string(proc_path);

namespace px {
namespace system {

const std::string& proc_path();

template <typename... Paths>
std::filesystem::path ProcPath(Paths... token_pack) {
  auto proc_pid_path = std::filesystem::path(proc_path());

  constexpr size_t n = sizeof...(token_pack);
  if constexpr (n > 0) {
    // GCC will complain about an unused variable (toks) if this code is *not*
    // inside of "if constexpr".
    std::filesystem::path toks[n] = {token_pack...};
    for (const auto& tok : toks) {
      proc_pid_path = proc_pid_path / tok.relative_path();
    }
  }
  return proc_pid_path;
}

template <typename... Paths>
std::filesystem::path ProcPidPath(pid_t pid, Paths... token_pack) {
  return ProcPath(std::to_string(pid), token_pack...);
}

template <typename... Paths>
std::filesystem::path ProcPidRootPath(pid_t pid, Paths... token_pack) {
  return ProcPidPath(pid, "root", token_pack...);
}

}  // namespace system
}  // namespace px
