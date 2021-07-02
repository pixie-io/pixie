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
#include <string>
#include <utility>

#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#include "src/stirling/bpf_tools/bcc_symbolizer.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/source_connectors/perf_profiler/symbol_cache.h"
#include "src/stirling/source_connectors/perf_profiler/types.h"

DECLARE_uint64(stirling_profiler_cache_eviction_threshold);

namespace px {
namespace stirling {

namespace profiler {
static constexpr upid_t kKernelUPID = {
    .pid = static_cast<uint32_t>(bpf_tools::BCCSymbolizer::kKernelPID), .start_time_ticks = 0};
}  // namespace profiler

/**
 * Symbolizer: provides an API to resolve a program address to a symbol.
 *
 * A typical use case looks like this:
 *   auto symbolize_fn = symbolizer.GetSymbolizerFn(upid);
 *   const std::string symbol = symbolize_fn(addr);
 */
class Symbolizer {
 public:
  virtual ~Symbolizer() = default;

  /**
   * Create a symbolizer for the process specified by UPID.
   * The returned symbolizer function converts addresses to symbols for the process.
   */
  virtual SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) = 0;

  /**
   * Delete the state associated with a symbolizer created by a previous call to GetSymbolizerFn
   */
  virtual void DeleteUPID(const struct upid_t& upid) = 0;
};

/**
 * A Symbolizer that uses BCC's symbolizer under the hood.
 */
class BCCSymbolizer : public Symbolizer, public NotCopyMoveable {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create();

  SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;
  void DeleteUPID(const struct upid_t& upid) override;

 private:
  BCCSymbolizer() = default;
  std::string_view Symbolize(const int pid, const uintptr_t addr);

  bpf_tools::BCCSymbolizer bcc_symbolizer_;
};

/**
 * A Symbolizer using the ElfReader symbolization core.
 */
class ElfSymbolizer : public Symbolizer, public NotCopyMoveable {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create();

  SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;
  void DeleteUPID(const struct upid_t& upid) override;

 private:
  ElfSymbolizer() = default;

  // A symbolizer per UPID.
  absl::flat_hash_map<struct upid_t,
                      std::unique_ptr<px::stirling::obj_tools::ElfReader::Symbolizer>>
      symbolizers_;
};

/**
 * A class that takes another symbolizer and adds a cache to it.
 */
class CachingSymbolizer : public Symbolizer {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create(std::unique_ptr<Symbolizer> inner_symbolizer);

  SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;

  void DeleteUPID(const struct upid_t& upid) override;
  size_t PerformEvictions();

  int64_t stat_accesses() const { return stat_accesses_; }
  int64_t stat_hits() const { return stat_hits_; }

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
