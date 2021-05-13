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

Status Symbolizer::Init() {
  // This BPF program is a placeholder; it is not actually used. We include it only
  // to allow us to create an instance of ebpf::BPFStackTable from BCC. We will use
  // the instance of ebpf::BPFStackTable for its symbolization API.
  // TODO(jps): find a symbolization API that does require a BPF program.
  const std::string_view kProgram = "BPF_STACK_TRACE(bcc_symbolizer, 16);";
  PL_RETURN_IF_ERROR(InitBPFProgram(kProgram));
  bcc_symbolizer_ = std::make_unique<ebpf::BPFStackTable>(GetStackTable("bcc_symbolizer"));
  return Status::OK();
}

void Symbolizer::FlushCache(const struct upid_t& upid) {
  // The inner map is owned by a unique_ptr; this will free the memory.
  symbol_caches_.erase(upid);

  // We also free up the symbol cache on the bcc side.
  // If the BCC side symbol cache has already been freed, this does nothing.
  // If later the pid is reused, then BCC will re-allocate the pid's symbol
  // symbol cache (when get_addr_symbol() is called).
  bcc_symbolizer_->free_symcache(upid.pid);
}

const std::string& Symbolizer::Symbolize(absl::flat_hash_map<uintptr_t, Symbol>* symbol_cache,
                                         const int pid, const uintptr_t addr) {
  static std::string symbol;
  if (!FLAGS_stirling_profiler_symcache) {
    symbol = bcc_symbolizer_->get_addr_symbol(addr, pid);
    return symbol;
  }

  ++stat_accesses_;

  // Symbol::Symbol(ebpf::BPFStackTable* bcc_symbolizer, const uintptr_t addr, const int pid)
  // will only be called if try_emplace fails to find a prexisting key. If no key is found,
  // the Symbol ctor uses bcc_symbolizer, and the addr & pid, to resolve the addr. to a symbol.
  const auto emplace_result = symbol_cache->try_emplace(addr, bcc_symbolizer_.get(), addr, pid);

  if (!emplace_result.second) {
    ++stat_hits_;
  }
  return emplace_result.first->second.symbol();
}

std::function<const std::string&(const uintptr_t addr)> Symbolizer::GetSymbolizerFn(
    const struct upid_t& upid) {
  using std::placeholders::_1;
  const auto [iter, inserted] = symbol_caches_.try_emplace(upid, nullptr);
  if (inserted) {
    using val_t = absl::flat_hash_map<uintptr_t, Symbol>;
    iter->second = std::make_unique<val_t>();
  }
  auto& cache = iter->second;
  auto fn = std::bind(&Symbolizer::Symbolize, this, cache.get(), upid.pid, _1);
  return fn;
}

}  // namespace stirling
}  // namespace px
