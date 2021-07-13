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

#include <vector>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/elf_tools.h"

extern "C" {
NO_OPT_ATTR uint32_t Trigger() { return 5; }
}

namespace px {
namespace stirling {

struct stack_trace_key_t {
  int32_t pid;
  int stack_trace_id;
};

const std::string_view kProgram = R"(
struct stack_trace_key_t {
  int32_t pid;
  int stack_trace_id;
};

BPF_STACK_TRACE(stack_traces, 16);
BPF_ARRAY(stack_trace_map, struct stack_trace_key_t, 1);

int sample_stack_trace(struct pt_regs* ctx) {
  int kIndex = 0;
  struct stack_trace_key_t* key = stack_trace_map.lookup(&kIndex);
  if (key == NULL) {
    return 0;
  }

  key->pid = bpf_get_current_pid_tgid() >> 32;
  key->stack_trace_id = stack_traces.get_stackid(ctx, BPF_F_USER_STACK);

  return 0;
}
)";

TEST(SymbolizerTest, SingleStackTrace) {
  bpf_tools::BCCWrapper bcc_wrapper;

  ASSERT_OK_AND_ASSIGN(std::filesystem::path self_path, fs::ReadSymlink("/proc/self/exe"));

  bpf_tools::UProbeSpec spec = {
      .binary_path = self_path.string(),
      .symbol = "Trigger",
      .probe_fn = "sample_stack_trace",
  };

  ASSERT_OK(bcc_wrapper.InitBPFProgram(kProgram));
  ASSERT_OK(bcc_wrapper.AttachUProbe(spec));

  // Run our BPF program, which should collect a stack trace.
  Trigger();

  // Get the stack trace ID from the BPF map.
  struct stack_trace_key_t val;
  auto stack_trace_table = bcc_wrapper.GetArrayTable<struct stack_trace_key_t>("stack_trace_map");
  stack_trace_table.get_value(0, val);

  // Get the list of addresses in the stack trace.
  auto stack_traces_table = bcc_wrapper.GetStackTable("stack_traces");
  std::vector<uintptr_t> addrs = stack_traces_table.get_stack_addr(val.stack_trace_id);

  // Create an ELF reader to symbolize the addresses.
  ASSERT_OK_AND_ASSIGN(auto elf_reader,
                       px::stirling::obj_tools::ElfReader::Create(self_path.string()));

  std::vector<std::string> symbols;
  for (const auto addr : addrs) {
    ASSERT_OK_AND_ASSIGN(auto sym, elf_reader->InstrAddrToSymbol(addr));
    symbols.push_back(sym.value_or("-"));
  }

#ifdef NDEBUG
  std::vector<std::string> expected_symbols = {
      "Trigger",
      "void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, "
      "void>(testing::Test*, void (testing::Test::*)(), char const*)",
      "testing::Test::Run()",
      "testing::TestInfo::Run()",
      "testing::TestSuite::Run()",
      "testing::internal::UnitTestImpl::RunAllTests()",
      "bool "
      "testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, "
      "bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char "
      "const*)",
      "testing::UnitTest::Run()",
      "main",
      "-"};
#else
  std::vector<std::string> expected_symbols = {
      "Trigger",
      "void testing::internal::HandleSehExceptionsInMethodIfSupported<testing::Test, "
      "void>(testing::Test*, void (testing::Test::*)(), char const*)",
      "void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, "
      "void>(testing::Test*, void (testing::Test::*)(), char const*)",
      "testing::Test::Run()",
      "testing::TestInfo::Run()",
      "testing::TestSuite::Run()",
      "testing::internal::UnitTestImpl::RunAllTests()",
      "bool "
      "testing::internal::HandleSehExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, "
      "bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char "
      "const*)",
      "bool "
      "testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, "
      "bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char "
      "const*)",
      "testing::UnitTest::Run()",
      "RUN_ALL_TESTS()",
      "main",
      "-"};
#endif

  EXPECT_THAT(symbols, ::testing::ContainerEq(expected_symbols));
}

}  // namespace stirling
}  // namespace px
