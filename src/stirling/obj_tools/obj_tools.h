#pragma once

#include <llvm-c/DisassemblerTypes.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/common.h"

namespace pl {
namespace stirling {
namespace obj_tools {

/**
 * Returns a path to the executable of the process specified by proc_pid.
 */
pl::StatusOr<std::filesystem::path> GetActiveBinary(std::filesystem::path host_path,
                                                    std::filesystem::path proc_pid);

// Note: GetActiveBinararies may seem unused, but is still used by code in experimental,
// so double-check before trying to remove.
/**
 * GetActiveBinaries returns the files pointed to by /proc/<pid>/exe, for all <pids>.
 *
 * @param pid_paths List of pids to process (see ::pl::system::ListProcPaths()).
 * @return a set of all active binaries.
 */
std::map<std::string, std::vector<int>> GetActiveBinaries(
    const std::map<int32_t, std::filesystem::path>& pid_paths,
    const std::filesystem::path& host_path = {});

/**
 * Initialize environment for LLVM disassembler APIs.
 * Can be called multiple times.
 */
void InitLLVMDisasm();

/**
 * RAII wrapper around LLVMDisasmContextRef.
 */
class LLVMDisasmContext {
 public:
  LLVMDisasmContext();
  ~LLVMDisasmContext();

  LLVMDisasmContextRef ref() const { return ref_; }

 private:
  LLVMDisasmContextRef ref_ = nullptr;
};

/**
 * Returns offset of all return instructions in the input byte code.
 */
std::vector<int> FindRetInsts(const LLVMDisasmContext& dcr, ::pl::utils::u8string_view byte_code);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
