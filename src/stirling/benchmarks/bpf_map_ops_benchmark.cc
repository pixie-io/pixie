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

#include <absl/strings/str_replace.h>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/utils/proc_path_tools.h"

using ::px::stirling::GetSelfPath;
using ::px::stirling::bpf_tools::BCCWrapper;
using ::px::stirling::bpf_tools::BPFProbeAttachType;
using ::px::stirling::bpf_tools::UProbeSpec;
using ::px::stirling::bpf_tools::WrappedBCCMap;

constexpr int kNumKeys = 100;

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant, but it must not be optimized away.
// We declare this with C linkage (extern "C") so it has a simple symbol name.
extern "C" {
NO_OPT_ATTR void BPFMapCleanupTrigger(int n, int* key_vec) {
  PX_UNUSED(n);
  PX_UNUSED(key_vec);
  return;
}

NO_OPT_ATTR void BPFMapPopulateTrigger(int n) {
  PX_UNUSED(n);
  return;
}
}

void SetupCleanupProbe(BCCWrapper* bcc) {
  std::filesystem::path self_path = GetSelfPath().ValueOrDie();
  auto elf_reader_or_s = px::stirling::obj_tools::ElfReader::Create(self_path.string());
  PX_CHECK_OK(elf_reader_or_s.status());
  auto elf_reader = elf_reader_or_s.ConsumeValueOrDie();

  const int64_t pid = getpid();
  auto converter_or_s = px::stirling::obj_tools::ElfAddressConverter::Create(elf_reader.get(), pid);
  PX_CHECK_OK(converter_or_s.status());
  auto converter = converter_or_s.ConsumeValueOrDie();

  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  auto symbol_addr =
      converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&BPFMapCleanupTrigger));

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = {},  // Keep GCC happy.
                    .address = symbol_addr,
                    .attach_type = BPFProbeAttachType::kEntry,
                    .probe_fn = "map_cleanup_uprobe"};

  PX_CHECK_OK(bcc->AttachUProbe(uprobe));
}

void SetupPopulateProbe(BCCWrapper* bcc) {
  std::filesystem::path self_path = GetSelfPath().ValueOrDie();
  auto elf_reader_or_s = px::stirling::obj_tools::ElfReader::Create(self_path.string());
  PX_CHECK_OK(elf_reader_or_s.status());
  auto elf_reader = elf_reader_or_s.ConsumeValueOrDie();

  const int64_t pid = getpid();
  auto converter_or_s = px::stirling::obj_tools::ElfAddressConverter::Create(elf_reader.get(), pid);
  PX_CHECK_OK(converter_or_s.status());
  auto converter = converter_or_s.ConsumeValueOrDie();

  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  auto symbol_addr =
      converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&BPFMapPopulateTrigger));

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = {},  // Keep GCC happy.
                    .address = symbol_addr,
                    .attach_type = BPFProbeAttachType::kEntry,
                    .probe_fn = "map_populate_uprobe"};

  PX_CHECK_OK(bcc->AttachUProbe(uprobe));
}

// NOLINTNEXTLINE : runtime/references.
static void BM_userspace_update_remove(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_HASH(map, int, int);";
  PX_CHECK_OK(bcc_wrapper.InitBPFProgram(kProgram));
  auto bpf_map = WrappedBCCMap<int, int>::Create(&bcc_wrapper, "map");

  for (auto _ : state) {
    for (int i = 0; i < kNumKeys; ++i) {
      PX_UNUSED(bpf_map->SetValue(i, 2 * i + 1));
    }

    for (int i = 0; i < kNumKeys; ++i) {
      PX_UNUSED(bpf_map->RemoveValue(i));
    }
  }

  for (int i = 0; i < kNumKeys; ++i) {
    CHECK(!bpf_map->GetValue(i).ok());
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_bpf_triggered_update_remove(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  std::string_view kProgram = R"(
#include <linux/ptrace.h>

BPF_HASH(map, int, int);

int map_cleanup_uprobe(struct pt_regs* ctx) {
  int n = (int)PT_REGS_PARM1(ctx);
  int* key_list = (int*)PT_REGS_PARM2(ctx);

bpf_trace_printk("cleanup %d\n", n);

#pragma unroll
  for (int i = 0; i < kNumKeys; ++i) {
    int key = key_list[i];

    if (i >= n) {
      break;
    }

    map.delete(&key);
  }

  return 0;
}

int map_populate_uprobe(struct pt_regs* ctx) {
  int n = (int)PT_REGS_PARM1(ctx);

  bpf_trace_printk("populate %d\n", n);


#pragma unroll
  for (int i = 0; i < kNumKeys; ++i) {
    int key = i;
    int val = 2 * i + 1;
    map.insert(&key, &val);
  }

  return 0;
}
)";
  // PX_CHECK_OK(bcc_wrapper.InitBPFProgram(kProgram, {absl::Substitute("-DkNumKeys=$0",
  // kNumKeys)}));
  PX_CHECK_OK(bcc_wrapper.InitBPFProgram(
      absl::StrReplaceAll(kProgram, {{"kNumKeys", std::to_string(kNumKeys)}})));
  auto bpf_map = WrappedBCCMap<int, int>::Create(&bcc_wrapper, "map");

  SetupCleanupProbe(&bcc_wrapper);
  SetupPopulateProbe(&bcc_wrapper);

  std::vector<int> keys;
  for (int i = 0; i < kNumKeys; ++i) {
    keys.push_back(i);
  }

  for (auto _ : state) {
    BPFMapPopulateTrigger(keys.size());
    BPFMapCleanupTrigger(keys.size(), keys.data());
  }

  for (int i = 0; i < kNumKeys; ++i) {
    CHECK(!bpf_map->GetValue(i).ok());
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_userspace_update_get_remove(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_HASH(map, int, int);";
  PX_CHECK_OK(bcc_wrapper.InitBPFProgram(kProgram));
  auto bpf_map = WrappedBCCMap<int, int>::Create(&bcc_wrapper, "map");

  for (auto _ : state) {
    for (int i = 0; i < kNumKeys; ++i) {
      PX_UNUSED(bpf_map->SetValue(i, 2 * i + 1));
    }

    for (int i = 0; i < kNumKeys; ++i) {
      auto status = bpf_map->GetValue(i);
      if (status.ok() && status.ValueOrDie() != 0) {
        PX_UNUSED(bpf_map->RemoveValue(i));
      }
    }
  }

  for (int i = 0; i < kNumKeys; ++i) {
    CHECK(!bpf_map->GetValue(i).ok());
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_bpf_triggered_update_get_remove(benchmark::State& state) {
  BCCWrapper bcc_wrapper;
  std::string_view kProgram = R"(
#include <linux/ptrace.h>

BPF_HASH(map, int, int);

int map_cleanup_uprobe(struct pt_regs* ctx) {
  int n = (int)PT_REGS_PARM1(ctx);
  int* key_list = (int*)PT_REGS_PARM2(ctx);

#pragma unroll
  for (int i = 0; i < kNumKeys; ++i) {
    int key = key_list[i];

    if (i >= n) {
      break;
    }

    int* val = map.lookup(&key);
    if (val != NULL && *val != 0) {
      map.delete(&key);
    }
  }

  return 0;
}

int map_populate_uprobe(struct pt_regs* ctx) {
  int n = (int)PT_REGS_PARM1(ctx);

#pragma unroll
  for (int i = 0; i < kNumKeys; ++i) {
    int key = i;
    int val = 2 * i + 1;
    map.update(&key, &val);
  }

  return 0;
}
)";
  PX_CHECK_OK(bcc_wrapper.InitBPFProgram(kProgram, {absl::Substitute("-DkNumKeys=$0", kNumKeys)}));
  auto bpf_map = WrappedBCCMap<int, int>::Create(&bcc_wrapper, "map");

  SetupCleanupProbe(&bcc_wrapper);
  SetupPopulateProbe(&bcc_wrapper);

  std::vector<int> keys;
  for (int i = 0; i < kNumKeys; ++i) {
    keys.push_back(i);
  }

  for (auto _ : state) {
    BPFMapPopulateTrigger(keys.size());
    BPFMapCleanupTrigger(keys.size(), keys.data());
  }

  for (int i = 0; i < kNumKeys; ++i) {
    CHECK(!bpf_map->GetValue(i).ok());
  }
}

BENCHMARK(BM_userspace_update_remove);
BENCHMARK(BM_bpf_triggered_update_remove);
BENCHMARK(BM_userspace_update_get_remove);
BENCHMARK(BM_bpf_triggered_update_get_remove);
