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

#include "src/stirling/obj_tools/init.h"

#include <llvm/Support/TargetSelect.h>

#include <mutex>

namespace px {
namespace stirling {

namespace {
#define InitTarget(TargetName)                  \
  do {                                          \
    LLVMInitialize##TargetName##Target();       \
    LLVMInitialize##TargetName##TargetInfo();   \
    LLVMInitialize##TargetName##TargetMC();     \
    LLVMInitialize##TargetName##AsmPrinter();   \
    LLVMInitialize##TargetName##AsmParser();    \
    LLVMInitialize##TargetName##Disassembler(); \
  } while (0)

void InitLLVMImpl() {
  InitTarget(X86);
  InitTarget(AArch64);
}
#undef InitTarget
}  // namespace

void InitLLVMOnce() {
  static std::once_flag initialized;
  std::call_once(initialized, InitLLVMImpl);
}

}  // namespace stirling
}  // namespace px
