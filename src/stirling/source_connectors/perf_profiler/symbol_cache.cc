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

#include "src/common/base/base.h"

namespace px {
namespace stirling {

SymbolCache::Symbol::Symbol(bpf_tools::BCCSymbolizer* bcc_symbolizer, const uintptr_t addr,
                            const int pid)
    : symbol_(bcc_symbolizer->SymbolOrAddrIfUnknown(addr, pid)) {}

SymbolCache::LookupResult SymbolCache::Lookup(const uintptr_t addr) {
  // Check old cache first, and move result to new cache if we have a hit.
  const auto prev_cache_iter = prev_cache_.find(addr);
  if (prev_cache_iter != prev_cache_.end()) {
    // Move to new cache.
    auto [curr_cache_iter, inserted] =
        cache_.try_emplace(addr, std::move(prev_cache_iter->second.symbol_));
    DCHECK(inserted);
    prev_cache_.erase(prev_cache_iter);
    return SymbolCache::LookupResult{curr_cache_iter->second.symbol_, true};
  }

  // If not in old cache, try the new cache (with automatic insertion if required).
  const auto [iter, inserted] = cache_.try_emplace(addr, symbolizer_, addr, pid_);
  return SymbolCache::LookupResult{iter->second.symbol_, !inserted};
}

}  // namespace stirling
}  // namespace px
