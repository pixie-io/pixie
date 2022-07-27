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

#include <linux/elf.h>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_symbolizer.h"

namespace px {
namespace stirling {
namespace bpf_tools {

BCCSymbolizer::BCCSymbolizer()
    : bcc_symbolization_options_({.use_debug_file = false,
                                  .check_debug_file_crc = false,
                                  .lazy_symbolize = true,
                                  .use_symbol_type = ((1 << STT_FUNC) | (1 << STT_GNU_IFUNC))}) {}

void* BCCSymbolizer::GetBCCSymbolCache(const int pid) {
  const auto [iter, inserted] = bcc_symbol_caches_by_pid_.try_emplace(pid, nullptr);

  if (inserted) {
    iter->second = bcc_symcache_new(pid, &bcc_symbolization_options_);
  }
  return iter->second;
}

std::string_view BCCSymbolizer::SymbolOrAddrIfUnknown(const int pid, const uintptr_t addr) {
  DCHECK(pid >= 0 || pid == -1);

  void* bcc_symbol_cache = GetBCCSymbolCache(pid);
  const bool resolved = 0 == bcc_symcache_resolve(bcc_symbol_cache, addr, &bcc_symbol_struct_);

  if (resolved) {
    // Symbol was resolved. Our work is done here.
    symbol_ = bcc_symbol_struct_.demangle_name;
    bcc_symbol_free_demangle_name(&bcc_symbol_struct_);

    return symbol_;
  }

  if (bcc_symbol_struct_.module != nullptr && strlen(bcc_symbol_struct_.module) > 0) {
    // Module is known (from /proc/<pid>/maps), but symbol is not known.
    // We will create a string like this:
    // [m] /lib/ld-musl-x86_64.so.1 + 0x0000abcd
    // This is better than nothing, but not as nice as a symbol.
    FormatModuleName(bcc_symbol_struct_.module, bcc_symbol_struct_.offset);
    return symbol_;
  }

  // If we fall through to here, we have truly come up empty handed.
  // Maybe it is a JIT'd or interpreted program? Maybe stack traces are broken (no frame pointers)?
  // We return just the virtual address, as a string, formatted in 64b hex.
  FormatAddress(addr);
  return symbol_;
}

void BCCSymbolizer::ReleasePIDSymCache(uint32_t pid) {
  auto iter = bcc_symbol_caches_by_pid_.find(pid);
  if (iter != bcc_symbol_caches_by_pid_.end()) {
    bcc_free_symcache(iter->second, pid);
    bcc_symbol_caches_by_pid_.erase(iter);
  }
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
