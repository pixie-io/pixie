#include "src/stirling/obj_tools/obj_tools.h"

#include <llvm-c/Disassembler.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/Support/TargetSelect.h>

#include <filesystem>
#include <memory>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/obj_tools/proc_path_tools.h"

namespace pl {
namespace stirling {
namespace obj_tools {

pl::StatusOr<std::filesystem::path> GetActiveBinary(std::filesystem::path host_path,
                                                    std::filesystem::path proc_pid) {
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, ResolveProcExe(proc_pid));

  // If we're running in a container, convert exe to be relative to our host mount.
  // Note that we mount host '/' to '/host' inside container.
  // Warning: must use JoinPath, because we are dealing with two absolute paths.
  std::filesystem::path host_exe = fs::JoinPath({&host_path, &proc_exe});
  PL_RETURN_IF_ERROR(fs::Exists(host_exe));
  return host_exe;
}

std::map<std::string, std::vector<int>> GetActiveBinaries(
    const std::map<int32_t, std::filesystem::path>& pid_paths,
    const std::filesystem::path& host_path) {
  std::map<std::string, std::vector<int>> binaries;
  for (const auto& [pid, p] : pid_paths) {
    VLOG(1) << absl::Substitute("Directory: $0", p.string());

    pl::StatusOr<std::filesystem::path> host_exe_or = GetActiveBinary(host_path, p);
    if (!host_exe_or.ok()) {
      VLOG(1) << absl::Substitute("Ignoring $0: Failed to resolve exe path, error message: $1",
                                  p.string(), host_exe_or.msg());
      continue;
    }

    binaries[host_exe_or.ValueOrDie()].push_back(pid);
  }

  LOG(INFO) << "Number of unique binaries found: " << binaries.size();
  for (const auto& b : binaries) {
    VLOG(1) << "  " << b.first;
  }

  return binaries;
}

void InitLLVMDisasm() {
  // It's legal to call these multiple times.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetDisassembler();
}

LLVMDisasmContext::LLVMDisasmContext() {
  // TripleName is ARCHITECTURE-VENDOR-OPERATING_SYSTEM.
  // See https://llvm.org/doxygen/Triple_8h_source.html
  // TODO(yzhao): Change to get TripleName from the system, instead of hard coding.
  ref_ = LLVMCreateDisasm(/*TripleName*/ "x86_64-pc-linux",
                          /*DisInfo*/ nullptr, /*TagType*/ 0, /*LLVMOpInfoCallback*/ nullptr,
                          /*LLVMSymbolLookupCallback*/ nullptr);
}

LLVMDisasmContext::~LLVMDisasmContext() { LLVMDisasmDispose(ref_); }

namespace {

bool IsRetInst(uint8_t code) {
  // https://c9x.me/x86/html/file_module_x86_id_280.html for full list.
  //
  // Near return to calling procedure.
  constexpr uint8_t kRetn = '\xc3';

  // Far return to calling procedure.
  constexpr uint8_t kRetf = '\xcb';

  // Near return to calling procedure and pop imm16 bytes from stack.
  constexpr uint8_t kRetnImm = '\xc2';

  // Far return to calling procedure and pop imm16 bytes from stack.
  constexpr uint8_t kRetfImm = '\xca';

  return code == kRetn || code == kRetf || code == kRetnImm || code == kRetfImm;
}

}  // namespace

std::vector<int> FindRetInsts(const LLVMDisasmContext& llvm_disam_ctx,
                              utils::u8string_view byte_code) {
  if (byte_code.empty()) {
    return {};
  }

  // Size of the buffer to hold disassembled assembly code. Since we do not really use the assembly
  // code, we just provide a small buffer.
  // (Unfortunately, nullptr and 0 crashes.)
  constexpr int kBufSize = 32;
  // Initialize array to zero. See more details at: https://stackoverflow.com/a/5591516.
  char buf[kBufSize] = {};

  int pc = 0;
  auto* codes = const_cast<uint8_t*>(byte_code.data());
  int codes_size = byte_code.size();
  int inst_size = 0;

  std::vector<int> res;
  do {
    if (IsRetInst(*codes)) {
      res.push_back(pc);
    }
    // TODO(yzhao): MCDisassembler::getInst() works better here, because it returns a MCInst, with
    // an opcode for examination. Unfortunately, MCDisassembler is difficult to create without
    // class LLVMDisasmContex, which is not exposed.
    inst_size = LLVMDisasmInstruction(llvm_disam_ctx.ref(), codes, codes_size, pc, buf, kBufSize);

    pc += inst_size;
    codes += inst_size;
    codes_size -= inst_size;
  } while (inst_size != 0);
  return res;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
