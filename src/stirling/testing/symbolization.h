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

#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

// Finds the address of a given kernel symbol from /proc/kallsyms.
StatusOr<uint64_t> GetKernelSymAddr(std::string_view symbol_name) {
  PX_ASSIGN_OR_RETURN(const std::string kallsyms, px::ReadFileToString("/proc/kallsyms"));

  // Example line from /proc/kallsyms:
  // ffffffffa60b0ee0 T __x64_sys_getpid
  for (std::string_view line : absl::StrSplit(kallsyms, "\n")) {
    if (absl::EndsWith(line, absl::Substitute(" T $0", symbol_name))) {
      std::vector<std::string_view> tokens = absl::StrSplit(line, " ");
      std::string addr_str(tokens[0]);
      return std::stoull(addr_str.data(), NULL, 16);
    }
  }

  return error::NotFound("Could not find $0", symbol_name);
}

}  // namespace stirling
}  // namespace px
