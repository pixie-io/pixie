#include "src/stirling/obj_tools/init.h"

#include <llvm/Support/TargetSelect.h>

#include <mutex>

namespace px {
namespace stirling {

namespace {
void InitLLVMImpl() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetDisassembler();
}
}  // namespace

void InitLLVMOnce() {
  static std::once_flag initialized;
  std::call_once(initialized, InitLLVMImpl);
}

}  // namespace stirling
}  // namespace px
