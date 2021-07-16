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

#include "src/stirling/bpf_tools/bcc_symbolizer.h"

#include <absl/strings/str_format.h>

namespace px {
namespace stirling {
namespace bpf_tools {

// A dummy BPF STACK_TRACE table descriptor.
// It's purpose is only to provide access to the BCC symbolizer.
// The table itself is a dummy and is never read/written, so its params mostly don't matter.
const ebpf::TableDesc kDummyTable(
    /* name */ "bogus",
    /* fd */ ebpf::FileDesc(-1), BPF_MAP_TYPE_STACK_TRACE,
    /* key_size */ sizeof(uint32_t),
    /* value_size */ sizeof(uint32_t),
    /* max_entries */ 1,
    /* flags */ 0);

BCCSymbolizer::BCCSymbolizer()
    : bcc_stack_table_(kDummyTable, /* use_debug_file */ false, /* check_debuf_file_crc */ false) {}

std::string BCCSymbolizer::Symbol(const uintptr_t addr, const int pid) {
  return bcc_stack_table_.get_addr_symbol(addr, pid);
}

std::string BCCSymbolizer::SymbolOrAddrIfUnknown(const uintptr_t addr, const int pid) {
  static constexpr std::string_view kUnknown = "[UNKNOWN]";
  std::string sym_or_addr = bcc_stack_table_.get_addr_symbol(addr, pid);
  if (sym_or_addr == kUnknown) {
    sym_or_addr = absl::StrFormat("0x%016llx", addr);
  }
  return sym_or_addr;
}

void BCCSymbolizer::ReleasePIDSymCache(uint32_t pid) { bcc_stack_table_.free_symcache(pid); }

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
