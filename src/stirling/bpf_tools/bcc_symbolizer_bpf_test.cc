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
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_symbolizer.h"

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

TEST(BCCSymbolizerTest, Symbol) {
  BCCSymbolizer symbolizer;

  pid_t pid = getpid();

  EXPECT_EQ(symbolizer.Symbol(kFooAddr, pid), "test::Foo()");
  EXPECT_EQ(symbolizer.Symbol(kBarAddr, pid), "test::Bar()");
  EXPECT_EQ(symbolizer.Symbol(123, pid), "[UNKNOWN]");
}

TEST(BCCSymbolizerTest, SymbolOrAddrIfUnknown) {
  BCCSymbolizer symbolizer;

  pid_t pid = getpid();

  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(kFooAddr, pid), "test::Foo()");
  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(kBarAddr, pid), "test::Bar()");
  EXPECT_EQ(symbolizer.SymbolOrAddrIfUnknown(123, pid), "0x000000000000007b");
}

// Finds the address of a given kernel symbol from /proc/kallsyms.
StatusOr<uint64_t> GetKernelSymInfo(std::string_view symbol_name) {
  PL_ASSIGN_OR_RETURN(const std::string kallsyms, px::ReadFileToString("/proc/kallsyms"));

  // Example line from /proc/kallsyms:
  // ffffffffa60b0ee0 T __x64_sys_getpid
  for (std::string_view line : absl::StrSplit(kallsyms, "\n")) {
    if (absl::EndsWith(line, absl::Substitute(" T $0", symbol_name))) {
      std::vector<std::string_view> tokens = absl::StrSplit(line, " ");
      std::string addr_str(tokens[0]);
      return std::stoull(addr_str.data(), NULL, 16);
    }
  }

  return error::NotFound("Could not find $0", symbol_name);
}

TEST(BCCSymbolizer, KernelSymbol) {
  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(uint64_t sym_addr, GetKernelSymInfo(kSymbolName));

  BCCSymbolizer symbolizer;
  EXPECT_EQ(symbolizer.Symbol(sym_addr, BCCSymbolizer::kKernelPID), kSymbolName);
  EXPECT_EQ(symbolizer.Symbol(0, BCCSymbolizer::kKernelPID), "[UNKNOWN]");
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
