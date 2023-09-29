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
#include "src/stirling/bpf_tools/rr/rr.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/utils/proc_path_tools.h"

using px::stirling::obj_tools::ElfAddressConverter;
using px::stirling::obj_tools::ElfReader;

// Create a std::string_view named rr_test_bcc_script based on the bazel target :rr_test_bpf_text.
// This is the BPF program we will invoke for this test.
OBJ_STRVIEW(rr_test_bcc_script, rr_test_bpf_text);

namespace test {

// We attach an eBPF user space probe to Foo() and Bar(). Later, we invoke Foo() and Bar()
// and expect that the eBPF recording mechanism records the perf buffer traffic generated
// by our eBPF probe.
NO_OPT_ATTR uint32_t Foo(const uint32_t arg) { return 1 + arg; }
NO_OPT_ATTR uint32_t Bar(const uint32_t arg) { return 2 + arg; }

std::vector<int> gold_data;
uint32_t test_idx = 0;

void PerfBufferRecordingDataFn(void* cb_cookie, void* data, int data_size) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  EXPECT_EQ(data_size, sizeof(int));
  int v = *static_cast<int*>(data);
  gold_data.push_back(v);
}

void PerfBufferReplayingDataFn(void* cb_cookie, void* data, int data_size) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  EXPECT_EQ(data_size, sizeof(int));
  int v = *static_cast<int*>(data);
  EXPECT_EQ(gold_data[test_idx], v);
  ++test_idx;
}

void PerfBufferLossFn(void* cb_cookie, uint64_t /*lost*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
}

}  // namespace test

namespace px {
namespace stirling {

using bpf_tools::BPFProbeAttachType;
using bpf_tools::UProbeSpec;
using bpf_tools::WrappedBCCArrayTable;
using bpf_tools::WrappedBCCMap;
using bpf_tools::WrappedBCCStackTable;

class BasicRecorderTest : public ::testing::Test {
 public:
  BasicRecorderTest() {}

 protected:
  void SetUp() override {
    recording_bcc_ = std::make_unique<bpf_tools::RecordingBCCWrapperImpl>();
    replaying_bcc_ = std::make_unique<bpf_tools::ReplayingBCCWrapperImpl>();

    // Register our BPF program in the kernel, for real (recording), and for fake (replaying).
    ASSERT_OK(recording_bcc_->InitBPFProgram(rr_test_bcc_script));
    ASSERT_OK(replaying_bcc_->InitBPFProgram(rr_test_bcc_script));

    const auto recording_perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>({
        .name = std::string("perf_buffer"),
        .probe_output_fn = test::PerfBufferRecordingDataFn,
        .probe_loss_fn = test::PerfBufferLossFn,
        .cb_cookie = this,
    });
    const auto replaying_perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>({
        .name = std::string("perf_buffer"),
        .probe_output_fn = test::PerfBufferReplayingDataFn,
        .probe_loss_fn = test::PerfBufferLossFn,
        .cb_cookie = this,
    });

    // Open perf buffers for real (recording), and for fake (replaying).
    ASSERT_OK(recording_bcc_->OpenPerfBuffers(recording_perf_buffer_specs));
    ASSERT_OK(replaying_bcc_->OpenPerfBuffers(replaying_perf_buffer_specs));

    const int64_t self_pid = getpid();
    const std::filesystem::path self_path = GetSelfPath().ValueOrDie();
    ASSERT_OK_AND_ASSIGN(auto elf_reader, ElfReader::Create(self_path.string()));
    ASSERT_OK_AND_ASSIGN(auto converter, ElfAddressConverter::Create(elf_reader.get(), self_pid));

    // For the uprobe spec.
    const uint64_t foo_virt_addr = reinterpret_cast<uint64_t>(&::test::Foo);
    const uint64_t bar_virt_addr = reinterpret_cast<uint64_t>(&::test::Bar);
    const uint64_t foo_bin_addr = converter->VirtualAddrToBinaryAddr(foo_virt_addr);
    const uint64_t bar_bin_addr = converter->VirtualAddrToBinaryAddr(bar_virt_addr);

    const UProbeSpec kFooUprobe{.binary_path = self_path,
                                .symbol = {},
                                .address = foo_bin_addr,
                                .attach_type = BPFProbeAttachType::kEntry,
                                .probe_fn = "push_something_into_a_perf_buffer"};
    const UProbeSpec kBarUprobe{.binary_path = self_path,
                                .symbol = {},
                                .address = bar_bin_addr,
                                .attach_type = BPFProbeAttachType::kEntry,
                                .probe_fn = "push_something_into_a_perf_buffer"};

    // Attach uprobes for this test case:
    ASSERT_OK(recording_bcc_->AttachUProbe(kFooUprobe));
    ASSERT_OK(recording_bcc_->AttachUProbe(kBarUprobe));
    ASSERT_OK(replaying_bcc_->AttachUProbe(kFooUprobe));
    ASSERT_OK(replaying_bcc_->AttachUProbe(kBarUprobe));
  }

  void TearDown() override {
    recording_bcc_->Close();
    replaying_bcc_->Close();
  }

  std::unique_ptr<bpf_tools::RecordingBCCWrapperImpl> recording_bcc_;
  std::unique_ptr<bpf_tools::ReplayingBCCWrapperImpl> replaying_bcc_;
};

class StackTableRecorderTest : public ::testing::Test {
 public:
  StackTableRecorderTest() {}

 protected:
  void SetUp() override {
    test::gold_data.clear();
    test::test_idx = 0;

    recording_bcc_ = std::make_unique<bpf_tools::RecordingBCCWrapperImpl>();
    replaying_bcc_ = std::make_unique<bpf_tools::ReplayingBCCWrapperImpl>();

    // Register our BPF program in the kernel, for real (recording), and for fake (replaying).
    ASSERT_OK(recording_bcc_->InitBPFProgram(rr_test_bcc_script));
    ASSERT_OK(replaying_bcc_->InitBPFProgram(rr_test_bcc_script));

    const auto recording_perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>({
        .name = std::string("stack_ids"),
        .probe_output_fn = test::PerfBufferRecordingDataFn,
        .probe_loss_fn = test::PerfBufferLossFn,
        .cb_cookie = this,
    });
    const auto replaying_perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>({
        .name = std::string("stack_ids"),
        .probe_output_fn = test::PerfBufferReplayingDataFn,
        .probe_loss_fn = test::PerfBufferLossFn,
        .cb_cookie = this,
    });

    // Open perf buffers for real (recording), and for fake (replaying).
    ASSERT_OK(recording_bcc_->OpenPerfBuffers(recording_perf_buffer_specs));
    ASSERT_OK(replaying_bcc_->OpenPerfBuffers(replaying_perf_buffer_specs));

    const int64_t self_pid = getpid();
    const std::filesystem::path self_path = GetSelfPath().ValueOrDie();
    ASSERT_OK_AND_ASSIGN(auto elf_reader, ElfReader::Create(self_path.string()));
    ASSERT_OK_AND_ASSIGN(auto converter, ElfAddressConverter::Create(elf_reader.get(), self_pid));

    // For the uprobe spec.
    const uint64_t foo_virt_addr = reinterpret_cast<uint64_t>(&::test::Foo);
    const uint64_t bar_virt_addr = reinterpret_cast<uint64_t>(&::test::Bar);
    const uint64_t foo_bin_addr = converter->VirtualAddrToBinaryAddr(foo_virt_addr);
    const uint64_t bar_bin_addr = converter->VirtualAddrToBinaryAddr(bar_virt_addr);

    const UProbeSpec kFooUprobe{.binary_path = self_path,
                                .symbol = {},
                                .address = foo_bin_addr,
                                .attach_type = BPFProbeAttachType::kEntry,
                                .probe_fn = "sample_a_stack_trace"};
    const UProbeSpec kBarUprobe{.binary_path = self_path,
                                .symbol = {},
                                .address = bar_bin_addr,
                                .attach_type = BPFProbeAttachType::kEntry,
                                .probe_fn = "sample_a_stack_trace"};

    // Attach uprobes for this test case:
    ASSERT_OK(recording_bcc_->AttachUProbe(kFooUprobe));
    ASSERT_OK(recording_bcc_->AttachUProbe(kBarUprobe));
    ASSERT_OK(replaying_bcc_->AttachUProbe(kFooUprobe));
    ASSERT_OK(replaying_bcc_->AttachUProbe(kBarUprobe));
  }

  void TearDown() override {
    recording_bcc_->Close();
    replaying_bcc_->Close();
  }

  std::unique_ptr<bpf_tools::RecordingBCCWrapperImpl> recording_bcc_;
  std::unique_ptr<bpf_tools::ReplayingBCCWrapperImpl> replaying_bcc_;
};

TEST_F(BasicRecorderTest, PerfBufferRRTest) {
  constexpr uint32_t kLoopIters = 16;

  for (uint32_t i = 0; i < kLoopIters; ++i) {
    // Invoking Foo() or Bar() triggers our eBPF uprobe, which will capture the function argument.
    PX_UNUSED(::test::Foo(2 * i + 0));
    PX_UNUSED(::test::Bar(2 * i + 1));
  }

  const std::string pb_file_name = "perf_buffer_replay_test.pb";

  // Polling perf buffers will cause the recording BCC wrapper to record each perf buffer event.
  recording_bcc_->PollPerfBuffers();

  // Write out the protobuf file and close the recording BCC wrapper.
  recording_bcc_->WriteProto(pb_file_name);
  recording_bcc_->Close();

  // Open the protobuf file in the replaying BCC wrapper.
  // The "replaying" data callback will check "test" (replay) data vs. "gold" data (captured
  // on the side during original recording when we invoked functions Foo() and Bar()).
  // Finally, we check that the "test" event count (from replay)
  // is the same as the "gold" event count (from recording).
  ASSERT_OK(replaying_bcc_->OpenReplayProtobuf(pb_file_name));
  replaying_bcc_->PollPerfBuffers();
  EXPECT_EQ(test::test_idx, test::gold_data.size());

  // TODO(jps): add the expectations.
}

TEST_F(BasicRecorderTest, BPFArrayRRTest) {
  auto recording_bpf_array = WrappedBCCArrayTable<int>::Create(recording_bcc_.get(), "results");

  constexpr uint32_t kLoopIters = 16;

  for (uint32_t i = 0; i < kLoopIters; ++i) {
    // Invoking Foo() or Bar() triggers our eBPF uprobe, which will capture the function argument.
    PX_UNUSED(::test::Foo(2 * i + 0));
    PX_UNUSED(::test::Bar(2 * i + 1));
  }
  for (uint32_t i = 0; i < 2 * kLoopIters; ++i) {
    ASSERT_OK_AND_ASSIGN(const int r, recording_bpf_array->GetValue(i));
    EXPECT_EQ(r, i);
  }

  constexpr int k100 = 0;
  constexpr int k200 = 100;
  ASSERT_OK(recording_bpf_array->SetValue(k100, 100));
  ASSERT_OK(recording_bpf_array->SetValue(k200, 200));

  {
    ASSERT_OK_AND_ASSIGN(const int r100, recording_bpf_array->GetValue(k100));
    ASSERT_OK_AND_ASSIGN(const int r200, recording_bpf_array->GetValue(k200));
    EXPECT_EQ(r100, 100);
    EXPECT_EQ(r200, 200);
  }

  const std::string pb_file_name = "bpf_array_replay_test.pb";

  // Write out the protobuf file and close the recording BCC wrapper.
  recording_bcc_->WriteProto(pb_file_name);
  recording_bcc_->Close();

  // Open the protobuf file in the replaying BCC wrapper.
  ASSERT_OK(replaying_bcc_->OpenReplayProtobuf(pb_file_name));

  // Get a pointer to the replay mode BPF map.
  auto replaying_bpf_array = WrappedBCCArrayTable<int>::Create(replaying_bcc_.get(), "results");

  // Test the first N map get value results.
  // These key/val pairs were set by triggering the BPF program.
  for (uint32_t i = 0; i < 2 * kLoopIters; ++i) {
    ASSERT_OK_AND_ASSIGN(const int r, replaying_bpf_array->GetValue(i));
    EXPECT_EQ(r, i);
  }

  // Test the the last two map get value results where we set the value from user space.
  {
    ASSERT_OK_AND_ASSIGN(const int r100, replaying_bpf_array->GetValue(k100));
    ASSERT_OK_AND_ASSIGN(const int r200, replaying_bpf_array->GetValue(k200));
    EXPECT_EQ(r100, 100);
    EXPECT_EQ(r200, 200);
  }
}

TEST_F(BasicRecorderTest, BPFMapRRTest) {
  auto recording_bpf_map = WrappedBCCMap<int, int>::Create(recording_bcc_.get(), "map");

  constexpr uint32_t kLoopIters = 16;

  for (uint32_t i = 0; i < kLoopIters; ++i) {
    // Invoking Foo() or Bar() triggers our eBPF uprobe, which will capture the function argument.
    PX_UNUSED(::test::Foo(2 * i + 0));
    PX_UNUSED(::test::Bar(2 * i + 1));
  }
  for (uint32_t i = 0; i < 2 * kLoopIters; ++i) {
    ASSERT_OK_AND_ASSIGN(const int r, recording_bpf_map->GetValue(i));
    EXPECT_EQ(r, i);
  }

  constexpr int k100 = 0;
  constexpr int k200 = 100;
  ASSERT_OK(recording_bpf_map->SetValue(k100, 100));
  ASSERT_OK(recording_bpf_map->SetValue(k200, 200));

  {
    ASSERT_OK_AND_ASSIGN(const int r100, recording_bpf_map->GetValue(k100));
    ASSERT_OK_AND_ASSIGN(const int r200, recording_bpf_map->GetValue(k200));
    EXPECT_EQ(r100, 100);
    EXPECT_EQ(r200, 200);
  }

  const std::string pb_file_name = "bpf_map_replay_test.pb";

  // Write out the protobuf file and close the recording BCC wrapper.
  recording_bcc_->WriteProto(pb_file_name);
  recording_bcc_->Close();

  // Open the protobuf file in the replaying BCC wrapper.
  ASSERT_OK(replaying_bcc_->OpenReplayProtobuf(pb_file_name));

  // Get a pointer to the replay mode BPF map.
  auto replaying_bpf_map = WrappedBCCMap<int, int>::Create(replaying_bcc_.get(), "map");

  // Test the first N map get value results.
  // These key/val pairs were set by triggering the BPF program.
  for (uint32_t i = 0; i < 2 * kLoopIters; ++i) {
    ASSERT_OK_AND_ASSIGN(const int r, replaying_bpf_map->GetValue(i));
    EXPECT_EQ(r, i);
  }

  // Test the the last two map get value results where we set the value from user space.
  {
    ASSERT_OK_AND_ASSIGN(const int r100, recording_bpf_map->GetValue(k100));
    ASSERT_OK_AND_ASSIGN(const int r200, recording_bpf_map->GetValue(k200));
    EXPECT_EQ(r100, 100);
    EXPECT_EQ(r200, 200);
  }
}

TEST_F(StackTableRecorderTest, BPFStackTableRRTest) {
  auto recording_stack_table = WrappedBCCStackTable::Create(recording_bcc_.get(), "stack_table");

  // Invoking Foo() or Bar() triggers our eBPF profiling probe.
  // Args to Foo() and Bar() will be totally ignored in this test case.
  constexpr int kIgnoredArg = 0;
  PX_UNUSED(::test::Foo(kIgnoredArg));
  PX_UNUSED(::test::Bar(kIgnoredArg));

  // Polling perf buffers will cause the recording BCC wrapper to record each perf buffer event.
  recording_bcc_->PollPerfBuffers();

  EXPECT_EQ(test::gold_data.size(), 2);
  const int stack_id_0 = test::gold_data[0];
  const int stack_id_1 = test::gold_data[1];

  constexpr bool kClearStackId = false;
  const auto gold_foo_stack_trace = recording_stack_table->GetStackAddr(stack_id_0, kClearStackId);
  const auto gold_bar_stack_trace = recording_stack_table->GetStackAddr(stack_id_1, kClearStackId);
  const uint64_t gold_foo_addr = gold_foo_stack_trace[0];
  const uint64_t gold_bar_addr = gold_bar_stack_trace[0];
  const auto gold_foo_symbol = recording_stack_table->GetAddrSymbol(gold_foo_addr, getpid());
  const auto gold_bar_symbol = recording_stack_table->GetAddrSymbol(gold_bar_addr, getpid());
  EXPECT_EQ(gold_foo_symbol, "test::Foo(unsigned int)");
  EXPECT_EQ(gold_bar_symbol, "test::Bar(unsigned int)");

  const std::string pb_file_name = "bpf_stack_table_replay_test.pb";

  // Write out the protobuf file and close the recording BCC wrapper.
  recording_bcc_->WriteProto(pb_file_name);
  recording_bcc_->Close();

  // Open the protobuf file in the replaying BCC wrapper.
  ASSERT_OK(replaying_bcc_->OpenReplayProtobuf(pb_file_name));

  // Create the replaying stack table.
  auto replaying_stack_table = WrappedBCCStackTable::Create(recording_bcc_.get(), "stack_table");

  // Replay perf buffer events. The callback fn. will invoke EXPECT vs. the gold data.
  replaying_bcc_->PollPerfBuffers();

  // Replay calls to GetStackAddr() and GetAddrSymbol().
  const auto test_foo_stack_trace = replaying_stack_table->GetStackAddr(stack_id_0, kClearStackId);
  const auto test_bar_stack_trace = replaying_stack_table->GetStackAddr(stack_id_1, kClearStackId);
  const uint64_t test_foo_addr = test_foo_stack_trace[0];
  const uint64_t test_bar_addr = test_bar_stack_trace[0];
  const auto test_foo_symbol = replaying_stack_table->GetAddrSymbol(test_foo_addr, getpid());
  const auto test_bar_symbol = replaying_stack_table->GetAddrSymbol(test_bar_addr, getpid());

  // Expect test & gold stack traces and symbols to match.
  EXPECT_EQ(test_foo_stack_trace.size(), gold_foo_stack_trace.size());
  EXPECT_EQ(test_bar_stack_trace.size(), gold_bar_stack_trace.size());

  for (uint32_t i = 0; i < test_foo_stack_trace.size(); ++i) {
    EXPECT_EQ(test_foo_stack_trace[i], gold_foo_stack_trace[i]);
  }
  for (uint32_t i = 0; i < test_bar_stack_trace.size(); ++i) {
    EXPECT_EQ(test_bar_stack_trace[i], gold_bar_stack_trace[i]);
  }

  EXPECT_EQ(gold_foo_addr, test_foo_addr);
  EXPECT_EQ(gold_bar_addr, test_bar_addr);
  EXPECT_EQ(gold_foo_symbol, test_foo_symbol);
  EXPECT_EQ(gold_bar_symbol, test_bar_symbol);
}

}  // namespace stirling
}  // namespace px
