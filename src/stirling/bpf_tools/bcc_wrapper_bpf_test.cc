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

#include "src/stirling/bpf_tools/bcc_wrapper.h"

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/system.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/testdata/dummy_exe_fixture.h"

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant, but it must not be optimized away.
// We declare this with C linkage (extern "C") so it has a simple symbol name.
extern "C" {
NO_OPT_ATTR void BCCWrapperTestProbeTrigger() { return; }
}

BPF_SRC_STRVIEW(get_tgid_start_time_bcc_script, get_tgid_start_time);

namespace px {
namespace stirling {
namespace bpf_tools {

using ::px::testing::BazelBinTestFilePath;
using ::px::testing::TempDir;

constexpr char kBCCProgram[] = R"BCC(
  int foo(struct pt_regs* ctx) {
    return 0;
  }
)BCC";

class DummyExeWrapper {
 public:
  DummyExeWrapper() {
    // Copy dummy_exe to temp directory so we can remove it to simulate non-existent file.
    const obj_tools::DummyExeFixture kDummyExeFixture;
    dummy_exe_path_ = temp_dir_.path() / "dummy_exe";
    PL_CHECK_OK(fs::Copy(kDummyExeFixture.Path(), dummy_exe_path_));
  }

  std::filesystem::path& path() { return dummy_exe_path_; }

 private:
  TempDir temp_dir_;
  std::filesystem::path dummy_exe_path_;
};

TEST(BCCWrapperTest, InitDefault) {
  // This will look for Linux Headers using the default strategy,
  // but since packaged headers are not included and PL_HOST_ENV is not defined,
  // it essentially boils down to a local headers search.
  // If the test host doesn't have Linux headers, we expect this test to fail.
  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));
}

TEST(BCCWrapperTest, InitWithTaskStructResolver) {
  // Force the TaskStructResolver to run, so we can test
  // that it doesn't lead to an infinite recursion.
  // The expected call stack is:
  //   BCCWrapper::InitBPFProgram
  //   TaskStructResolver
  //   BCCWrapper::InitBPFProgram
  //   <end> (The second instance of BCCWrapper shouldn't call TaskStructResolver again.)
  FLAGS_stirling_always_infer_task_struct_offsets = true;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));
}

TEST(BCCWrapperTest, DetachUProbe) {
  DummyExeWrapper dummy_exe;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));

  UProbeSpec spec = {
      .binary_path = dummy_exe.path(),
      .symbol = "CanYouFindThis",
      .probe_fn = "foo",
  };

  {
    ASSERT_OK(bcc_wrapper.AttachUProbe(spec));
    EXPECT_EQ(1, bcc_wrapper.num_attached_probes());

    ASSERT_OK(bcc_wrapper.DetachUProbe(spec));
    EXPECT_EQ(0, bcc_wrapper.num_attached_probes());
  }

  {
    ASSERT_OK(bcc_wrapper.AttachUProbe(spec));
    EXPECT_EQ(1, bcc_wrapper.num_attached_probes());

    // Remove the binary.
    ASSERT_OK(fs::Remove(dummy_exe.path()));
    ASSERT_OK(bcc_wrapper.DetachUProbe(spec));
    EXPECT_EQ(0, bcc_wrapper.num_attached_probes());
  }
}

TEST(BCCWrapperTest, GetTGIDStartTime) {
  // Force the TaskStructResolver to run,
  // since we're trying to check that it correctly gets the task_struct offsets.
  FLAGS_stirling_always_infer_task_struct_offsets = true;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(get_tgid_start_time_bcc_script));

  // Get the PID start time from /proc.
  ASSERT_OK_AND_ASSIGN(uint64_t expected_proc_pid_start_time,
                       ::px::system::GetPIDStartTimeTicks("/proc/self"));

  ASSERT_OK_AND_ASSIGN(std::filesystem::path self_path, fs::ReadSymlink("/proc/self/exe"));

  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  uint64_t symbol_addr = reinterpret_cast<uint64_t>(&BCCWrapperTestProbeTrigger);

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = {},  // Keep GCC happy.
                    .address = symbol_addr,
                    .attach_type = BPFProbeAttachType::kEntry,
                    .probe_fn = "probe_tgid_start_time"};

  ASSERT_OK(bcc_wrapper.AttachUProbe(uprobe));

  // Trigger our uprobe.
  BCCWrapperTestProbeTrigger();

  auto tgid_start_time_output = bcc_wrapper.GetArrayTable<uint64_t>("tgid_start_time_output");
  uint64_t proc_pid_start_time;
  ASSERT_TRUE(tgid_start_time_output.get_value(0, proc_pid_start_time).ok());

  EXPECT_EQ(proc_pid_start_time, expected_proc_pid_start_time);
}

TEST(BCCWrapperTest, TestMapClearingAPIs) {
  // Test to show that get_table_offline() with clear_table=true actually clears the table.
  bpf_tools::BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_HASH(alphabet, char const * const, uint64_t, 26);";
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kProgram));
  ebpf::BPFHashTable alphabet = bcc_wrapper.GetHashTable<const char*, uint64_t>("alphabet");

  // Set the expected values in the BPF hash table:
  ASSERT_TRUE(alphabet.update_value("a", 0).ok());
  ASSERT_TRUE(alphabet.update_value("b", 1).ok());
  ASSERT_TRUE(alphabet.update_value("c", 2).ok());
  ASSERT_TRUE(alphabet.update_value("d", 3).ok());

  using ::testing::IsEmpty;
  using ::testing::Pair;
  using ::testing::UnorderedElementsAre;

  auto gold = UnorderedElementsAre(Pair("a", 0), Pair("b", 1), Pair("c", 2), Pair("d", 3));

  // Verify that get_table_offline() returns the values we expect:
  ASSERT_THAT(alphabet.get_table_offline(), gold);

  // Verify calling get_table_offline() (again) returns the values we expect,
  // but this time we are passing in parameter clear_table=true:
  constexpr bool kClearTable = true;
  ASSERT_THAT(alphabet.get_table_offline(kClearTable), gold);

  // After calling get_table_offline() with clear_table=true,
  // the table should be empty.
  ASSERT_THAT(alphabet.get_table_offline(), IsEmpty());
}

// Tests that BCCWrapper can load XDP program.
TEST(BCCWrapperTest, LoadXDP) {
  bpf_tools::BCCWrapper bcc_wrapper;

  // TODO(yzhao): Changing to TCP does not work as expected. Needs more research.
  std::string_view xdp_program = R"bcc(
      #include <linux/bpf.h>
      #include <linux/if_ether.h>
      #include <linux/in.h>
      #include <linux/ip.h>
      #include <linux/udp.h>

      int udpfilter(struct xdp_md *ctx) {
        bpf_trace_printk("got a packet\n");
        void *data = (void *)(long)ctx->data;
        void *data_end = (void *)(long)ctx->data_end;
        struct ethhdr *eth = data;
        if ((void *)eth + sizeof(*eth) <= data_end) {
          struct iphdr *ip = data + sizeof(*eth);
          if ((void *)ip + sizeof(*ip) <= data_end) {
            if (ip->protocol == IPPROTO_UDP) {
              struct udphdr *udp = (void *)ip + sizeof(*ip);
              if ((void *)udp + sizeof(*udp) <= data_end) {
                if (udp->dest == ntohs(7998)) {
                  bpf_trace_printk("udp port 7999\n");
                  udp->dest = ntohs(7999);
                }
              }
            }
          }
        }
        return XDP_PASS;
      }
  )bcc";
  ASSERT_OK(bcc_wrapper.InitBPFProgram(xdp_program));
  ASSERT_OK(bcc_wrapper.AttachXDP("lo", "udpfilter"));
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
