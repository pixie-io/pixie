#include "src/stirling/bpf_tools/utils.h"

#include <utility>

namespace pl {
namespace stirling {
namespace bpf_tools {

StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(
    const UProbeSpec& spec, const obj_tools::ElfReader::SymbolInfo& target,
    obj_tools::ElfReader* elf_reader) {
  DCHECK(spec.attach_type == BPFProbeAttachType::kReturn);

  PL_ASSIGN_OR_RETURN(std::vector<uint64_t> ret_inst_addrs, elf_reader->FuncRetInstAddrs(target));

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
  PL_ASSIGN_OR_RETURN(const std::vector<obj_tools::ElfReader::SymbolInfo> symbol_infos,
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
}  // namespace pl
