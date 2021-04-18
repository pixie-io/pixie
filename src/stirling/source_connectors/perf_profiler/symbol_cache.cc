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

#include "src/stirling/source_connectors/perf_profiler/symbol_cache.h"

DEFINE_bool(stirling_profiler_symcache, true, "Enable the Stirling managed symbol cache.");

namespace px {
namespace stirling {

namespace {
const std::string& SymbolOrAddr(ebpf::BPFStackTable* stack_traces, const uintptr_t addr,
                                const int pid) {
  static constexpr std::string_view kUnknown = "[UNKNOWN]";
  static std::string sym_or_addr;
  sym_or_addr = stack_traces->get_addr_symbol(addr, pid);
  if (sym_or_addr == kUnknown) {
    sym_or_addr = absl::StrFormat("0x%016llx", addr);
  }
  return sym_or_addr;
}
}  // namespace

const std::string& SymbolCache::LookupSym(ebpf::BPFStackTable* stack_traces, const uintptr_t addr) {
  if (!FLAGS_stirling_profiler_symcache) {
    return SymbolOrAddr(stack_traces, addr, pid_);
  }

  ++stat_accesses_;

  auto sym_iter = sym_cache_.find(addr);
  if (sym_iter == sym_cache_.end()) {
    sym_iter = sym_cache_.try_emplace(addr, SymbolOrAddr(stack_traces, addr, pid_)).first;
  } else {
    ++stat_hits_;
  }
  return sym_iter->second;
}

void SymbolCache::Flush() { sym_cache_.clear(); }

}  // namespace stirling
}  // namespace px
