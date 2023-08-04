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

#include <benchmark/benchmark.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/obj_tools/elf_reader.h"

using ::px::stirling::bpf_tools::BCCWrapper;
using ::px::stirling::bpf_tools::WrappedBCCArrayTable;
using ::px::stirling::bpf_tools::WrappedBCCStackTable;
using ::px::stirling::obj_tools::ElfReader;

extern "C" {
NO_OPT_ATTR uint32_t Trigger() { return 5; }
}

struct stack_trace_key_t {
  int32_t pid;
  int stack_trace_id;
};

const std::string_view kProgram = R"(
  #include <linux/bpf_perf_event.h>
  #include <linux/ptrace.h>

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

std::vector<uintptr_t> CollectStackTrace(BCCWrapper* bcc_wrapper,
                                         const std::filesystem::path& self_path) {
  ::px::stirling::bpf_tools::UProbeSpec spec = {
      .binary_path = self_path.string(),
      .symbol = "Trigger",
      .probe_fn = "sample_stack_trace",
  };

  PX_CHECK_OK(bcc_wrapper->InitBPFProgram(kProgram));
  PX_CHECK_OK(bcc_wrapper->AttachUProbe(spec));

  // Run our BPF program, which should collect a stack trace.
  Trigger();

  // Get the stack trace ID from the BPF map.
  auto stack_trace_table =
      WrappedBCCArrayTable<struct stack_trace_key_t>::Create(bcc_wrapper, "stack_trace_map");
  const struct stack_trace_key_t val = stack_trace_table->GetValue(0).ConsumeValueOrDie();

  // Get the list of addresses in the stack trace.
  auto stack_traces_table = WrappedBCCStackTable::Create(bcc_wrapper, "stack_traces");
  return stack_traces_table->GetStackAddr(val.stack_trace_id, /* clear_stack_id */ false);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_elf_reader_symbolization(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  PX_ASSIGN_OR_EXIT(std::filesystem::path self_path, ::px::fs::ReadSymlink("/proc/self/exe"));
  std::vector<uintptr_t> addrs = CollectStackTrace(&bcc_wrapper, self_path);
  PX_ASSIGN_OR_EXIT(auto elf_reader,
                    ::px::stirling::obj_tools::ElfReader::Create(self_path.string()));
  const uint64_t self_pid = getpid();
  PX_ASSIGN_OR_EXIT(auto converter, ::px::stirling::obj_tools::ElfAddressConverter::Create(
                                        elf_reader.get(), self_pid));

  for (auto _ : state) {
    std::vector<std::string> symbols;
    for (const auto addr : addrs) {
      auto binary_addr = converter->VirtualAddrToBinaryAddr(addr);
      PX_ASSIGN_OR_EXIT(auto sym, elf_reader->InstrAddrToSymbol(binary_addr));
      symbols.push_back(sym.value_or("-"));
    }
    benchmark::DoNotOptimize(symbols);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_elf_reader_symbolization_indexed(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  PX_ASSIGN_OR_EXIT(std::filesystem::path self_path, ::px::fs::ReadSymlink("/proc/self/exe"));
  std::vector<uintptr_t> addrs = CollectStackTrace(&bcc_wrapper, self_path);
  PX_ASSIGN_OR_EXIT(auto elf_reader, ElfReader::Create(self_path.string()));
  const uint64_t self_pid = getpid();
  PX_ASSIGN_OR_EXIT(auto converter, ::px::stirling::obj_tools::ElfAddressConverter::Create(
                                        elf_reader.get(), self_pid));

  PX_ASSIGN_OR_EXIT(std::unique_ptr<ElfReader::Symbolizer> symbolizer, elf_reader->GetSymbolizer());

  for (auto _ : state) {
    std::vector<std::string> symbols;
    for (const auto addr : addrs) {
      auto binary_addr = converter->VirtualAddrToBinaryAddr(addr);
      symbols.push_back(std::string(symbolizer->Lookup(binary_addr)));
    }
    benchmark::DoNotOptimize(symbols);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_bcc_symbolization(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  PX_ASSIGN_OR_EXIT(std::filesystem::path self_path, ::px::fs::ReadSymlink("/proc/self/exe"));
  std::vector<uintptr_t> addrs = CollectStackTrace(&bcc_wrapper, self_path);
  auto bcc_symbolizer = WrappedBCCStackTable::Create(&bcc_wrapper, "stack_traces");
  const int32_t pid = getpid();

  for (auto _ : state) {
    std::vector<std::string> symbols;
    for (const auto addr : addrs) {
      auto sym = bcc_symbolizer->GetAddrSymbol(addr, pid);
      symbols.push_back(sym);
    }
    benchmark::DoNotOptimize(symbols);
  }
}

BENCHMARK(BM_bcc_symbolization);
BENCHMARK(BM_elf_reader_symbolization);
BENCHMARK(BM_elf_reader_symbolization_indexed);
