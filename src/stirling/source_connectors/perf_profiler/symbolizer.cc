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

#include <utility>

#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

DEFINE_bool(stirling_profiler_symcache, true, "Enable the Stirling managed symbol cache.");

namespace px {
namespace stirling {

StatusOr<std::unique_ptr<Symbolizer>> BCCSymbolizer::Create() {
  return std::unique_ptr<Symbolizer>(new BCCSymbolizer());
}

void BCCSymbolizer::DeleteUPID(const struct upid_t& upid) {
  // The inner map is owned by a unique_ptr; this will free the memory.
  symbol_caches_.erase(upid);

  // We also free up the symbol cache on the BCC side.
  // If the BCC side symbol cache has already been freed, this does nothing.
  // If later the pid is reused, then BCC will re-allocate the pid's symbol
  // symbol cache (when get_addr_symbol() is called).
  bcc_symbolizer_.ReleasePIDSymCache(upid.pid);
}

std::string_view BCCSymbolizer::Symbolize(SymbolCache* symbol_cache, const int pid,
                                          const uintptr_t addr) {
  static std::string symbol;
  if (!FLAGS_stirling_profiler_symcache) {
    symbol = bcc_symbolizer_.Symbol(addr, pid);
    return symbol;
  }

  ++stat_accesses_;

  // Symbol::Symbol(ebpf::BPFStackTable* bcc_symbolizer, const uintptr_t addr, const int pid)
  // will only be called if try_emplace fails to find a prexisting key. If no key is found,
  // the Symbol ctor uses bcc_symbolizer, and the addr & pid, to resolve the addr. to a symbol.
  const SymbolCache::LookupResult result = symbol_cache->Lookup(addr);

  if (result.hit) {
    ++stat_hits_;
  }
  return result.symbol;
}

std::function<std::string_view(const uintptr_t addr)> BCCSymbolizer::GetSymbolizerFn(
    const struct upid_t& upid) {
  using std::placeholders::_1;
  const auto [iter, inserted] = symbol_caches_.try_emplace(upid, nullptr);
  if (inserted) {
    iter->second = std::make_unique<SymbolCache>(upid.pid, &bcc_symbolizer_);
  }
  auto& cache = iter->second;
  auto fn =
      std::bind(&BCCSymbolizer::Symbolize, this, cache.get(), static_cast<int32_t>(upid.pid), _1);
  return fn;
}

}  // namespace stirling
}  // namespace px
