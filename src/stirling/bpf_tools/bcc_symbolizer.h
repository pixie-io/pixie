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

#include <bcc/BPF.h>
// Including bcc/BPF.h creates some conflicts with our own code.
#ifdef DECLARE_ERROR
#undef DECLARE_ERROR
#endif

#include <string>

namespace px {
namespace stirling {
namespace bpf_tools {

/**
 * A wrapper around ebpf::BPFStackTable to gain access to the BCC symbolization API.
 * The table itself is a dummy and is not used.
 */
class BCCSymbolizer {
 public:
  BCCSymbolizer();

  /**
   * PID used to denote a kernel-space address.
   */
  static inline constexpr int kKernelPID = -1;

  /**
   * Convert the address for the process with the provided PID into a symbol.
   */
  std::string Symbol(const uintptr_t addr, const int pid);

  /**
   * Like Symbol(), but if the symbol is not resolved, returns a string of the address.
   */
  std::string SymbolOrAddrIfUnknown(const uintptr_t addr, const int pid);

  /**
   * Release any cached resources, such as loaded symbol tables, for the given PID.
   * If the PID is accessed again, the required data will be loaded again.
   */
  void ReleasePIDSymCache(uint32_t pid);

 private:
  ebpf::BPFStackTable bcc_stack_table_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
