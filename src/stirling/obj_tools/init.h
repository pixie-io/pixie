#pragma once

namespace px {
namespace stirling {

/**
 * Initialize LLVM.
 * Required by:
 *  - ElfReader disassembler APIs.
 *  - DwarfReader.
 *
 * Can be called multiple times. Will only initialize once.
 */
void InitLLVMOnce();

}  // namespace stirling
}  // namespace px
