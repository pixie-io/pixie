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

#include "src/stirling/bpf_tools/utils.h"

#include <utility>

namespace px {
namespace stirling {
namespace bpf_tools {

StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(
    const UProbeSpec& spec, const obj_tools::ElfReader::SymbolInfo& target,
    obj_tools::ElfReader* elf_reader) {
  DCHECK(spec.attach_type == BPFProbeAttachType::kReturn);

  PX_ASSIGN_OR_RETURN(std::vector<uint64_t> ret_inst_addrs, elf_reader->FuncRetInstAddrs(target));

  std::vector<UProbeSpec> res;

  for (const uint64_t& addr : ret_inst_addrs) {
    UProbeSpec spec_cpy = spec;
    spec_cpy.attach_type = BPFProbeAttachType::kEntry;
    spec_cpy.address = addr;
    // Also remove the predefined symbol.
    spec_cpy.symbol.clear();
    res.push_back(std::move(spec_cpy));
  }

  return res;
}

StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(const UProbeSpec& spec,
                                                             obj_tools::ElfReader* elf_reader) {
  PX_ASSIGN_OR_RETURN(const std::vector<obj_tools::ElfReader::SymbolInfo> symbol_infos,
                      elf_reader->ListFuncSymbols(spec.symbol, obj_tools::SymbolMatchType::kExact));

  if (symbol_infos.empty()) {
    return error::NotFound("Symbol '$0' was not found", spec.symbol);
  }

  if (symbol_infos.size() > 1) {
    return error::NotFound("Symbol '$0' appeared $1 times", spec.symbol);
  }

  return TransformGolangReturnProbe(spec, symbol_infos.front(), elf_reader);
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
