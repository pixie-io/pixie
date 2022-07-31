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

#include <bcc/bcc_syms.h>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_format.h>

// Including bcc/BPF.h creates some conflicts with our own code.
#ifdef DECLARE_ERROR
#undef DECLARE_ERROR
#endif

#include <string>

namespace px {
namespace stirling {
namespace bpf_tools {

/**
 * A wrapper around the symbolization library functions provided by BCC.
 */
class BCCSymbolizer {
 public:
  BCCSymbolizer();

  /**
   * PID used to denote a kernel-space address.
   */
  static inline constexpr int kKernelPID = -1;

  /**
   * Like Symbol(), but if the symbol is not resolved, returns a string of the address.
   */
  std::string_view SymbolOrAddrIfUnknown(int pid, uintptr_t addr);

  /**
   * Release any cached resources, such as loaded symbol tables, for the given PID.
   * If the PID is accessed again, the required data will be loaded again.
   */
  void ReleasePIDSymCache(uint32_t pid);

 private:
  /**
   * FormatModuleName(): populates symbol_ when the memory region is known, but symbol is not.
   */
  void FormatModuleName(char const* const module, const uintptr_t offset) {
    symbol_ = absl::StrFormat("[m] %s + 0x%08llx", module, offset);
  }

  /**
   * FormatAddress(): populates symbol_ with a stringified addr. (64b hex).
   * used when no memory region or symbol known is known.
   */
  void FormatAddress(const uintptr_t addr) { symbol_ = absl::StrFormat("0x%016llx", addr); }

  /**
   * GetBCCSymbolCache() returns a pointer to a BCC symbol cache. Use of void* is inherited
   * from the BCC library. The BCC symbol cache itself is not really a cache, rather, it is
   * a two level index, first of memory regions (found in /proc/<pid>/maps) and second of
   * the symbols within each mapped memory region.
   */
  void* GetBCCSymbolCache(const int pid);

  /**
   * Options that impact the behavior of the underlying BCC symbolizer.
   */
  bcc_symbol_option bcc_symbolization_options_;

  /**
   * bcc_symbol_caches_by_pid_ maps from pid to BCC symbol cache. Use of void* is inherited
   * from the BCC library.
   */
  absl::flat_hash_map<int, void*> bcc_symbol_caches_by_pid_;

  /**
   * bcc_symbol_struct_ contains information about the symbol found for a given virtual address.
   * It may have a completely resolved symbol, just the module (i.e. memory region found from
   * /proc/<pid>/maps), or neither of those. bcc_symbol_struct_ is populated when we call the
   * bcc_symcache_resolve() method from the BCC library.
   */
  bcc_symbol bcc_symbol_struct_;

  /**
   * symbol_ allocates a std::string where we populate the result
   * of calling BCCSymbolizer::SymbolOrAddrIfUnknown(), i.e. storage for the result of invoking
   * the main public API of this class.
   */
  std::string symbol_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
