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

#include "src/stirling/source_connectors/perf_profiler/symbol_cache/symbol_cache.h"

#include "src/common/base/base.h"

namespace px {
namespace stirling {

SymbolCache::Symbol::Symbol(profiler::SymbolizerFn symbolizer_fn, const uintptr_t addr)
    : symbol_(symbolizer_fn(addr)) {}

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
  const auto [iter, inserted] = cache_.try_emplace(addr, symbolizer_fn_, addr);
  return SymbolCache::LookupResult{iter->second.symbol_, !inserted};
}

size_t SymbolCache::PerformEvictions() {
  size_t evict_count = prev_cache_.size();
  prev_cache_ = std::move(cache_);
  cache_.clear();
  return evict_count;
}

}  // namespace stirling
}  // namespace px
