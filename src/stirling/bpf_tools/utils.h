#pragma once

#include <vector>

#include "src/common/base/statusor.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/elf_tools.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

/**
 * Returns a list of UProbeSpec duplicated from the input UProbeSpec, whose addresses are the return
 * addresses of the function specified by the target. The input elf_reader is used to parse the
 * binary.
 */
StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(
    const UProbeSpec& spec, const elf_tools::ElfReader::SymbolInfo& target,
    elf_tools::ElfReader* elf_reader);

/**
 * Wraps the above function. The additional functionality is to obtain SymbolInfo from the symbol
 * address of the input spec.
 */
StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(const UProbeSpec& spec,
                                                             elf_tools::ElfReader* elf_reader);

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl
