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

#include "src/stirling/bpf_tools/bcc_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_bool(stirling_profiler_symcache, true, "Enable the Stirling managed symbol cache.");

using ::px::stirling::obj_tools::ElfReader;

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

std::string_view BCCSymbolizer::SymbolizeCached(SymbolCache* symbol_cache, const uintptr_t addr) {
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

std::string_view BCCSymbolizer::SymbolizeUncached(const uintptr_t addr, const int pid) {
  static std::string symbol;
  symbol = bcc_symbolizer_.SymbolOrAddrIfUnknown(addr, pid);
  return symbol;
}

SymbolizerFn BCCSymbolizer::GetSymbolizerFn(const struct upid_t& upid) {
  using std::placeholders::_1;

  SymbolizerFn fn =
      std::bind(&BCCSymbolizer::SymbolizeUncached, this, _1, static_cast<int32_t>(upid.pid));
  if (!FLAGS_stirling_profiler_symcache) {
    return fn;
  }

  // If caching is enabled, we return a different symbolizer function that goes through the cache.
  // The cache, however, uses the same basic function that we define above.
  const auto [iter, inserted] = symbol_caches_.try_emplace(upid, nullptr);
  if (inserted) {
    iter->second = std::make_unique<SymbolCache>(fn);
  }
  auto& cache = iter->second;
  return std::bind(&BCCSymbolizer::SymbolizeCached, this, cache.get(), _1);
}

StatusOr<std::unique_ptr<Symbolizer>> ElfSymbolizer::Create() {
  ElfSymbolizer* elf_symbolizer = new ElfSymbolizer();
  auto symbolizer = std::unique_ptr<Symbolizer>(elf_symbolizer);
  return symbolizer;
}

void ElfSymbolizer::DeleteUPID(const struct upid_t& upid) { symbolizers_.erase(upid); }

StatusOr<std::unique_ptr<ElfReader::Symbolizer>> CreateUPIDSymbolizer(const struct upid_t& upid) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<FilePathResolver> fp_resolver,
                      FilePathResolver::Create(upid.pid));
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, ProcExe(upid.pid));
  PL_ASSIGN_OR_RETURN(std::filesystem::path host_proc_exe, fp_resolver->ResolvePath(proc_exe));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(host_proc_exe));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<ElfReader::Symbolizer> upid_symbolizer,
                      elf_reader->GetSymbolizer());
  return upid_symbolizer;
}

std::string_view EmptySymbolizerFn(const uintptr_t) { return ""; }
std::string_view DummyKernelSymbolizerFn(const uintptr_t) { return "<kernel symbol>"; }

SymbolizerFn ElfSymbolizer::GetSymbolizerFn(const struct upid_t& upid) {
  constexpr uint32_t kKernelPID = static_cast<uint32_t>(-1);
  if (upid.pid == kKernelPID) {
    return SymbolizerFn(&(DummyKernelSymbolizerFn));
  }

  std::unique_ptr<ElfReader::Symbolizer>& upid_symbolizer = symbolizers_[upid];
  if (upid_symbolizer == nullptr) {
    StatusOr<std::unique_ptr<ElfReader::Symbolizer>> upid_symbolizer_status =
        CreateUPIDSymbolizer(upid);
    if (!upid_symbolizer_status.ok()) {
      LOG(ERROR) << absl::Substitute("Failed to create Symbolizer function for $0 [error=$1]",
                                     upid.pid, upid_symbolizer_status.ToString());
      return SymbolizerFn(&(EmptySymbolizerFn));
    }

    upid_symbolizer = upid_symbolizer_status.ConsumeValueOrDie();
  }

  return std::bind(&ElfReader::Symbolizer::Lookup, upid_symbolizer.get(), std::placeholders::_1);
}

}  // namespace stirling
}  // namespace px
