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

#include <sys/syscall.h>

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"
#include "src/stirling/source_connectors/perf_profiler/shared/symbolization.h"
#include "src/stirling/source_connectors/perf_profiler/stringifier.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/bcc_symbolizer.h"
#include "src/stirling/utils/proc_path_tools.h"

// Create a std::string_view named stringifer_test_bcc_script based
// on the bazel target :stringifier_test_bpf_text.
// The BPF program samples a call stack, and stores the resulting stack-id
// into a shared BPF stack traces table.
OBJ_STRVIEW(stringifer_test_bcc_script, stringifier_test_bpf_text);

using ::testing::AnyOfArray;
using ::testing::StartsWith;

namespace test {

// Foo() and Bar() are used to inject some "known" symbols
// into the folded stack trace string returned by the stringifer.
// For this, the test attaches a uprobe to Foo() & Bar(),
// and attaches a kprobe to the getpid syscall.
NO_OPT_ATTR uint32_t Bar() { return getpid(); }

NO_OPT_ATTR uint32_t Foo() { return getpid(); }

}  // namespace test

namespace px {
namespace stirling {

using bpf_tools::WrappedBCCMap;

namespace {
// MakeUserStackTraceKey, MakeKernStackTraceKey, and MakeUserKernStackTraceKey
// are convenience functions that construct stack trace histogram keys
// based on pid+stack-id. In this test, the notion of upid (pid+start_time_ticks)
// is not needed, so we coerce start_time_ticks to zero. We also populate
// any unused stack-id with the appropriate error code (-EFAULT) so that the
// stringifier does not fire a DCHECK.
stack_trace_key_t MakeUserStackTraceKey(const uint32_t pid, const int stack_id) {
  const stack_trace_key_t key = {.upid = {.pid = pid, .start_time_ticks = 0ULL},
                                 .user_stack_id = stack_id,
                                 .kernel_stack_id = -EFAULT};
  return key;
}

stack_trace_key_t MakeKernStackTraceKey(const uint32_t pid, const int stack_id) {
  const stack_trace_key_t key = {.upid = {.pid = pid, .start_time_ticks = 0ULL},
                                 .user_stack_id = -EFAULT,
                                 .kernel_stack_id = stack_id};
  return key;
}

stack_trace_key_t MakeUserKernStackTraceKey(const uint32_t pid, const int u_stack_id,
                                            const int k_stack_id) {
  const stack_trace_key_t key = {.upid = {.pid = pid, .start_time_ticks = 0ULL},
                                 .user_stack_id = u_stack_id,
                                 .kernel_stack_id = k_stack_id};
  return key;
}

// These are the expected leaf symbols in our user space folded stack trace strings.
const std::set<std::string> kPossibleUSyms = {"test::Bar()", "test::Foo()", "__getpid", "getpid"};
const std::set<std::string> kPossibleKSyms = {"[k] __x64_sys_getpid", "[k] __ia32_sys_getpid",
                                              "[k] sys_getpid", "[k] __do_sys_getpid"};

}  // namespace

class StringifierTest : public ::testing::Test {
 public:
  using StringVec = std::vector<std::string>;
  using AddrVec = std::vector<uintptr_t>;
  using Key = stack_trace_key_t;
  using Histogram = WrappedBCCMap<Key, uint64_t>;

  StringifierTest() {}

 protected:
  void SetUp() override {
    // Register our BPF program in the kernel.
    ASSERT_OK(bcc_wrapper_.InitBPFProgram(stringifer_test_bcc_script));

    // Bind the BCC API to the shared BPF maps created by our BPF program.
    stack_traces_ = WrappedBCCStackTable::Create(&bcc_wrapper_, "stack_traces");
    histogram_ = Histogram::Create(&bcc_wrapper_, "histogram");

    // Create a symbolizer (needed for the stringifer).
    ASSERT_OK_AND_ASSIGN(symbolizer_, BCCSymbolizer::Create());

    // Create our device under test, the stringifier.
    // It needs a symbolizer and a shared BPF stack traces map.
    stringifier_ =
        std::make_unique<Stringifier>(symbolizer_.get(), symbolizer_.get(), stack_traces_.get());
  }

  void TearDown() override {}

  void PopulatedFoldedStringsMap(const int stack_id, const Key& key, const bool is_kernel) {
    if (!all_stack_ids_.contains(stack_id)) {
      // Get the folded stack trace string, and store it in the folded_strings_map_.
      const std::string stack_trace_str = stringifier_->FoldedStackTraceString(key);
      folded_strings_map_[stack_id] = stack_trace_str;
      VLOG(1) << absl::Substitute("stack_id: $0, stack_trace_str: $1", stack_id, stack_trace_str);

      // Tokenize the folded stack trace string.
      const StringVec symbols = absl::StrSplit(stack_trace_str, symbolization::kSeparator);

      const auto expected_symbols = is_kernel ? kPossibleKSyms : kPossibleUSyms;
      const auto prefix = is_kernel ? symbolization::kKernelPrefix : symbolization::kUserPrefix;

      // Check that each token begins with the appropriate prefix.
      for (const auto& symbol : symbols) {
        ASSERT_THAT(symbol, StartsWith(std::string(prefix)));
      }

      // Check that the leaf symbol in the stack trace string is in our expected set.
      ASSERT_THAT(symbols.back(), AnyOfArray(expected_symbols));

      // Populate the stack id sets.
      absl::flat_hash_set<int>& stack_id_set = is_kernel ? k_stack_ids_ : u_stack_ids_;
      all_stack_ids_.insert(stack_id);
      stack_id_set.insert(stack_id);
    } else {
      // This folded stack trace string has already been populated;
      // Check that the stringifer correctly memoized its previous result.
      const std::string gold = folded_strings_map_[stack_id];
      const std::string test = stringifier_->FoldedStackTraceString(key);
      ASSERT_EQ(test, gold);
      ++num_stack_ids_reused_;
    }
  }

  uint64_t num_stack_ids_reused_ = 0;

  bpf_tools::BCCWrapper bcc_wrapper_;
  std::unique_ptr<WrappedBCCStackTable> stack_traces_;
  std::unique_ptr<Histogram> histogram_;

  std::unique_ptr<Symbolizer> symbolizer_;
  std::unique_ptr<Stringifier> stringifier_;

  // Sets of observed stack-ids for user, kernel, and their union.
  // We use these to check that the stringifier returns memoized stack trace strings,
  // even after clearing the BPF shared stack traces map.
  absl::flat_hash_set<int> k_stack_ids_;
  absl::flat_hash_set<int> u_stack_ids_;
  absl::flat_hash_set<int> all_stack_ids_;

  absl::flat_hash_map<int, std::string> folded_strings_map_;
};

TEST_F(StringifierTest, MemoizationTest) {
  const std::filesystem::path self_path = GetSelfPath().ValueOrDie();
  ASSERT_OK_AND_ASSIGN(auto elf_reader, obj_tools::ElfReader::Create(self_path.string()));
  const int64_t self_pid = getpid();
  ASSERT_OK_AND_ASSIGN(auto converter,
                       obj_tools::ElfAddressConverter::Create(elf_reader.get(), self_pid));
  // Values used in creating the [u|k] probe specs.
  const uint64_t foo_addr =
      converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&::test::Foo));
  const uint64_t bar_addr =
      converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&::test::Bar));

  // uprobe specs, for Foo() and Bar(). We invoke our BPF program,
  // stack_trace_sampler, when Foo() or Bar() is called.
  const bpf_tools::UProbeSpec kFooUprobe{.binary_path = self_path,
                                         .symbol = {},
                                         .address = foo_addr,
                                         .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
                                         .probe_fn = "stack_trace_sampler"};
  const bpf_tools::UProbeSpec kBarUprobe{.binary_path = self_path,
                                         .symbol = {},
                                         .address = bar_addr,
                                         .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
                                         .probe_fn = "stack_trace_sampler"};

  // kprobe spec. to attach our BPF program, stack_trace_sampler, to syscall getpid.
  constexpr bpf_tools::KProbeSpec kPidKprobe{"getpid", bpf_tools::BPFProbeAttachType::kEntry,
                                             "stack_trace_sampler"};

  // Attach uprobes & kprobes for this test case:
  ASSERT_OK(bcc_wrapper_.AttachKProbe(kPidKprobe));
  ASSERT_OK(bcc_wrapper_.AttachUProbe(kFooUprobe));
  ASSERT_OK(bcc_wrapper_.AttachUProbe(kBarUprobe));

  // Foo() & Bar() tickle our uprobes and kprobe. Both simply return the pid.
  uint32_t pid;
  pid = ::test::Foo();
  pid = ::test::Bar();

  // Detach the probes now that we have collected data.
  // ... later, we will verify that the stringifier cleared the stack traces map,
  // as it is required to do. And for this verification to work, we need to
  // stop collecting data now.
  bcc_wrapper_.Close();

  // Used below, in calls into BCC APIs.
  constexpr bool kClearTable = true;
  constexpr bool kNoClearStackId = false;

  // Move the stack trace histogram out of the BPF shared map into our local map.
  const auto histo = histogram_->GetTableOffline(kClearTable);

  // Use the stringifier to populate folded_strings_map_ (used for memoization check, later).
  // Inside of PopulatedFoldedStringsMap(), we check the following invariants:
  // ... that each symbolized token begins with the proper [u|k] prefix.
  // ... that the leaf symbolized token is in the expected set based on our test call stack.
  for (const auto& [key, count] : histo) {
    if (key.upid.pid == pid) {
      const int u_stack_id = key.user_stack_id;
      const int k_stack_id = key.kernel_stack_id;

      if (u_stack_id >= 0) {
        constexpr bool is_kernel = false;
        const Key key = MakeUserStackTraceKey(pid, u_stack_id);
        ASSERT_NO_FATAL_FAILURE(PopulatedFoldedStringsMap(u_stack_id, key, is_kernel));
      }
      if (k_stack_id >= 0) {
        constexpr bool is_kernel = true;
        const Key key = MakeKernStackTraceKey(pid, k_stack_id);
        ASSERT_NO_FATAL_FAILURE(PopulatedFoldedStringsMap(k_stack_id, key, is_kernel));
      }
    } else {
      // This will clear the stack-traces table for both the user & kernel
      // stack-id inside of the stack-trace-key.
      stringifier_->FoldedStackTraceString(key);
    }
  }

  // We expect that the stack-id sets are populated.
  EXPECT_GT(u_stack_ids_.size(), 0);
  EXPECT_GT(k_stack_ids_.size(), 0);
  EXPECT_GT(all_stack_ids_.size(), 0);

  // This test is designed to reuse at least one kernel stack-id.
  VLOG(1) << "num_stack_ids_reused_: " << num_stack_ids_reused_;
  EXPECT_GT(num_stack_ids_reused_, 0);

  // The stringifier is responsible for clearing the BPF shared stack traces map;
  // by this point in the test, the stack traces map should be fully cleared.
  // We test that functionality here.
  for (const int stack_id : all_stack_ids_) {
    const AddrVec expected_empty = stack_traces_->GetStackAddr(stack_id, kNoClearStackId);
    EXPECT_EQ(expected_empty.size(), 0) << stack_id;
  }

  // The stringifier memoizes its results to cover the case where stack-ids are reused.
  // Reuse of stack-ids is common, and can look like this:
  // (user-stack-id, kernel-stack-id)
  // (1, 2) no reuse yet...
  // (1, 3) reused stack-id 1 with different kernel stack-id (different leaf in same syscall?)
  // (4, 2) reused stack-id 2 (reuse of kernel stack-ids is demonstrated in this test)
  // (1, -EFAULT) reused stack-id 1, w/ no kernel stack trace (because not in kernel)
  // (1, -EEXIST) reused stack-id 1, w/ no kernel stack trace (because of hash collision)

  // Here, we verify that the stringifier has indeed memoized its results.
  // NB: we already verified that the stack traces map is empty, so the stringifier
  // cannot be reading from it.
  for (const int k_stack_id : k_stack_ids_) {
    for (const int u_stack_id : u_stack_ids_) {
      const stack_trace_key_t key = MakeUserKernStackTraceKey(pid, u_stack_id, k_stack_id);
      const std::string kSep = std::string(symbolization::kSeparator);
      const std::string k_gold = folded_strings_map_[k_stack_id];
      const std::string u_gold = folded_strings_map_[u_stack_id];
      const std::string gold = u_gold + kSep + k_gold;
      const std::string test = stringifier_->FoldedStackTraceString(key);
      EXPECT_EQ(test, gold);
    }
  }
}

TEST_F(StringifierTest, KernelDropMessageTest) {
  const pid_t pid = getpid();

  // If the BPF stack-trace-histogram hash table has a collision, the stack-id
  // is populated with -EEXIST.
  const Key dropped_key = MakeUserKernStackTraceKey(pid, -EEXIST, -EEXIST);
  const std::string dropped = stringifier_->FoldedStackTraceString(dropped_key);
  VLOG(1) << "kernel drop message: " << dropped;
  EXPECT_EQ(dropped, symbolization::kDropMessage);
}

}  // namespace stirling
}  // namespace px
