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

#include <absl/functional/bind_front.h>

#include "src/stirling/source_connectors/perf_profiler/symbolizers/caching_symbolizer.h"

DEFINE_uint64(
    stirling_profiler_cache_eviction_threshold, 1000,
    "Number of symbols in the current generation of the cache that triggers an eviction.");

namespace px {
namespace stirling {

StatusOr<std::unique_ptr<Symbolizer>> CachingSymbolizer::Create(
    std::unique_ptr<Symbolizer> inner_symbolizer) {
  auto ptr = new CachingSymbolizer();
  auto uptr = std::unique_ptr<Symbolizer>(ptr);
  ptr->symbolizer_ = std::move(inner_symbolizer);
  return uptr;
}

void CachingSymbolizer::IterationPreTick() { symbolizer_->IterationPreTick(); }

profiler::SymbolizerFn CachingSymbolizer::GetSymbolizerFn(const struct upid_t& upid) {
  if (symbolizer_->Uncacheable(upid)) {
    // NB: the normal cache eviction process will eventually zero out the memory used
    // by the cache that was created while the Java symbolizer was in its "attach" phase.
    return symbolizer_->GetSymbolizerFn(upid);
  }

  const auto [iter, inserted] = symbol_caches_.try_emplace(upid, nullptr);

  // Here, we trigger the get symbolizer logic in the underlying symbolizer to ensure that
  // we catch any Java processes that may have had their agent delayed by attach rate limiting.
  auto symbolizer_fn = symbolizer_->GetSymbolizerFn(upid);

  if (inserted) {
    iter->second = std::make_unique<SymbolCache>(symbolizer_fn);
  }
  auto& cache = iter->second;

  // TODO(jps): Remove this extra 'set_symbolizer_fn()' when we deprecate agent rate limiting.
  cache->set_symbolizer_fn(symbolizer_fn);

  auto fn = absl::bind_front(&CachingSymbolizer::Symbolize, this, cache.get());
  return fn;
}

void CachingSymbolizer::DeleteUPID(const struct upid_t& upid) {
  // The inner map is owned by a unique_ptr; this will free the memory.
  symbol_caches_.erase(upid);

  symbolizer_->DeleteUPID(upid);
}

size_t CachingSymbolizer::PerformEvictions() {
  // Zero has a special meaning: no evictions.
  if (FLAGS_stirling_profiler_cache_eviction_threshold == 0) {
    return 0;
  }

  size_t active_entries = 0;
  for (const auto& sym_cache : symbol_caches_) {
    active_entries += sym_cache.second->active_entries();
  }

  size_t evict_count = 0;
  if (active_entries > FLAGS_stirling_profiler_cache_eviction_threshold) {
    for (const auto& sym_cache : symbol_caches_) {
      evict_count += sym_cache.second->PerformEvictions();
    }
  }

  return evict_count;
}

std::string_view CachingSymbolizer::Symbolize(SymbolCache* symbol_cache, const uintptr_t addr) {
  ++stat_accesses_;

  const SymbolCache::LookupResult result = symbol_cache->Lookup(addr);

  if (result.hit) {
    ++stat_hits_;
  }
  return result.symbol;
}

uint64_t CachingSymbolizer::GetNumberOfSymbolsCached() const {
  uint64_t n = 0;
  for (const auto& [upid, symbol_cache] : symbol_caches_) {
    n += symbol_cache->total_entries();
  }
  return n;
}

}  // namespace stirling
}  // namespace px
