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

#include "src/stirling/source_connectors/perf_profiler/symbol_cache/symbol_cache.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/symbolizer.h"

DECLARE_uint64(stirling_profiler_cache_eviction_threshold);

namespace px {
namespace stirling {

/**
 * A class that takes another symbolizer and adds a cache to it.
 */
class CachingSymbolizer : public Symbolizer {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create(std::unique_ptr<Symbolizer> inner_symbolizer);

  profiler::SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;

  void DeleteUPID(const struct upid_t& upid) override;
  void IterationPreTick() override;
  size_t PerformEvictions();

  int64_t stat_accesses() const { return stat_accesses_; }
  int64_t stat_hits() const { return stat_hits_; }
  uint64_t GetNumberOfSymbolsCached() const;
  bool Uncacheable(const struct upid_t& /*upid*/) override { return false; }

 private:
  CachingSymbolizer() = default;

  std::string_view Symbolize(SymbolCache* symbol_cache, const uintptr_t addr);

  std::unique_ptr<Symbolizer> symbolizer_;

  absl::flat_hash_map<struct upid_t, std::unique_ptr<SymbolCache>> symbol_caches_;

  int64_t stat_accesses_ = 0;
  int64_t stat_hits_ = 0;
};

}  // namespace stirling
}  // namespace px
