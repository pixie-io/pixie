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

#include <gtest/gtest.h>

#include "src/common/base/error.h"
#include "src/common/base/file.h"
#include "src/common/system/proc_parser.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_symbolizer.h"
#include "src/stirling/testing/symbolization.h"

// Some functions for which we'll lookup symbols by address.
namespace test {
void Foo() { LOG(INFO) << "foo()."; }
void Bar() { LOG(INFO) << "bar()."; }
}  // namespace test

const uintptr_t kFooAddr = reinterpret_cast<uintptr_t>(&test::Foo);
const uintptr_t kBarAddr = reinterpret_cast<uintptr_t>(&test::Bar);

namespace px {
namespace stirling {
namespace bpf_tools {

TEST(BCCSymbolizer, SymbolOrAddrIfUnknown) {
  BCCSymbolizer symbolizer;

  const pid_t pid = getpid();

  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(pid, kFooAddr), "test::Foo()");
  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(pid, kBarAddr), "test::Bar()");
  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(pid, 123), "0x000000000000007b");
}

TEST(BCCSymbolizer, KernelSymbol) {
  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(uint64_t sym_addr, GetKernelSymAddr(kSymbolName));

  BCCSymbolizer symbolizer;
  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(BCCSymbolizer::kKernelPID, sym_addr), kSymbolName);
  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(BCCSymbolizer::kKernelPID, 0), "0x0000000000000000");
}

TEST(BCCSymbolizer, ModuleName) {
  BCCSymbolizer symbolizer;
  std::vector<system::ProcParser::ProcessSMaps> smaps;

  const pid_t pid = getpid();
  const system::ProcParser proc_parser;
  ASSERT_OK(proc_parser.ParseProcPIDSMaps(pid, &smaps));

  for (const auto& entry : smaps) {
    if (entry.pathname == "[vdso]") {
      const std::string_view symbol = symbolizer.SymbolOrAddrIfUnknown(pid, entry.vmem_start);
      EXPECT_EQ(symbol, "[m] [vdso] + 0x00000000");
    }
  }
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
