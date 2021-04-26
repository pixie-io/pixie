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

#include <string>

#include "src/stirling/bpf_tools/bcc_wrapper.h"

DECLARE_bool(stirling_profiler_symcache);

namespace px {
namespace stirling {

/**
 * Symbolizer, based on its "enable_symbolization" policy either:
 * ... provides a resolved symbol based on (pid, address), or,
 * ... returns a stringified version of the address.
 *
 * When enable_symbolization=true, Symbolizer uses BCC to resolve the
 * symbols in the underlying object code.
 #
 * If FLAGS_stirling_profiler_symcache==true, Symbolizer
 * keeps its own symbol cache to reduce the cost of symbol lookup.
 * While BCC has its own symbol cache, the bcc cache is expensive to use.
 *
 */
class Symbolizer {
 public:
  explicit Symbolizer(const int pid, const bool enable_symbolization)
      : pid_(pid), enable_symbolization_(enable_symbolization) {}

  const std::string& LookupSym(ebpf::BPFStackTable* stack_traces, const uintptr_t addr);

  void FlushCache();

  int64_t stat_accesses() { return stat_accesses_; }
  int64_t stat_hits() { return stat_hits_; }

  // BCC's symbol resolver assumes the kernel PID is -1.
  static constexpr int kKernelPID = -1;

 private:
  const int pid_;
  const bool enable_symbolization_;

  absl::flat_hash_map<uintptr_t, std::string> sym_cache_;

  int64_t stat_accesses_ = 0;
  int64_t stat_hits_ = 0;
};

}  // namespace stirling
}  // namespace px
