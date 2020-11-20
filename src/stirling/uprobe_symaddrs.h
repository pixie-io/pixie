#pragma once

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/symaddrs.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"

namespace pl {
namespace stirling {

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for Go HTTP2
 * uprobe deployment.
 */
StatusOr<struct go_common_symaddrs_t> GoCommonSymAddrs(elf_tools::ElfReader* elf_reader,
                                                       dwarf_tools::DwarfReader* dwarf_reader);

/**
 * Uses ELF and DWARF information to return the locations of all relevant symbols for Go HTTP2
 * uprobe deployment.
 */
StatusOr<struct go_http2_symaddrs_t> GoHTTP2SymAddrs(elf_tools::ElfReader* elf_reader,
                                                     dwarf_tools::DwarfReader* dwarf_reader);

}  // namespace stirling
}  // namespace pl
