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

#include <unistd.h>
#include <chrono>
#include <thread>
#include "src/stirling/bpf_tools/bcc_wrapper.h"

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/system.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/obj_tools/testdata/cc/test_exe_fixture.h"

// A function which we will uprobe on, to trigger our BPF code.
// The function itself is irrelevant, but it must not be optimized away.
// We declare this with C linkage (extern "C") so it has a simple symbol name.
extern "C" {
NO_OPT_ATTR void BCCWrapperTestProbeTrigger() { return; }
}

OBJ_STRVIEW(get_tgid_start_time_bcc_script, get_tgid_start_time);

namespace px {
namespace stirling {
namespace bpf_tools {

using ::px::testing::BazelRunfilePath;
using ::px::testing::TempDir;

constexpr char kBCCProgram[] = R"BCC(
  int foo(struct pt_regs* ctx) {
    return 0;
  }
)BCC";

class TestExeWrapper {
 public:
  TestExeWrapper() {
    // Copy test_exe to temp directory so we can remove it to simulate non-existent file.
    const obj_tools::TestExeFixture kTestExeFixture;
    test_exe_path_ = temp_dir_.path() / "test_exe";
    PX_CHECK_OK(fs::Copy(kTestExeFixture.Path(), test_exe_path_));
  }

  std::filesystem::path& path() { return test_exe_path_; }

 private:
  TempDir temp_dir_;
  std::filesystem::path test_exe_path_;
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

  std::vector<std::string> cflags = {};
  bool requires_linux_headers = true;
  bool always_infer_task_struct_offsets = true;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram, cflags, requires_linux_headers,
                                       always_infer_task_struct_offsets));
}

TEST(BCCWrapperTest, DetachUProbe) {
  TestExeWrapper test_exe;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));

  UProbeSpec spec = {
      .binary_path = test_exe.path(),
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
    ASSERT_OK(fs::Remove(test_exe.path()));
    ASSERT_OK(bcc_wrapper.DetachUProbe(spec));
    EXPECT_EQ(0, bcc_wrapper.num_attached_probes());
  }
}

TEST(BCCWrapperTest, GetTGIDStartTime) {
  // Force the TaskStructResolver to run,
  // since we're trying to check that it correctly gets the task_struct offsets.
  std::vector<std::string> cflags = {};
  bool requires_linux_headers = true;
  bool always_infer_task_struct_offsets = true;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(get_tgid_start_time_bcc_script, cflags,
                                       requires_linux_headers, always_infer_task_struct_offsets));

  // Get the PID start time from /proc.
  ASSERT_OK_AND_ASSIGN(uint64_t expected_proc_pid_start_time,
                       ::px::system::GetPIDStartTimeTicks("/proc/self"));

  ASSERT_OK_AND_ASSIGN(std::filesystem::path self_path, fs::ReadSymlink("/proc/self/exe"));

  ASSERT_OK_AND_ASSIGN(auto elf_reader, obj_tools::ElfReader::Create(self_path.string()));
  const int64_t self_pid = getpid();
  ASSERT_OK_AND_ASSIGN(auto converter,
                       obj_tools::ElfAddressConverter::Create(elf_reader.get(), self_pid));

  // Use address instead of symbol to specify this probe,
  // so that even if debug symbols are stripped, the uprobe can still attach.
  uint64_t symbol_addr =
      converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&BCCWrapperTestProbeTrigger));

  UProbeSpec uprobe{.binary_path = self_path,
                    .symbol = {},  // Keep GCC happy.
                    .address = symbol_addr,
                    .attach_type = BPFProbeAttachType::kEntry,
                    .probe_fn = "probe_tgid_start_time"};

  ASSERT_OK(bcc_wrapper.AttachUProbe(uprobe));

  // Trigger our uprobe.
  BCCWrapperTestProbeTrigger();

  auto tgid_start_times =
      WrappedBCCArrayTable<uint64_t>::Create(&bcc_wrapper, "tgid_start_time_output");
  ASSERT_OK_AND_ASSIGN(const uint64_t proc_pid_start_time, tgid_start_times->GetValue(0));

  EXPECT_EQ(proc_pid_start_time, expected_proc_pid_start_time);
}

TEST(BCCWrapperTest, TestMapClearingAPIs) {
  // Test to show that get_table_offline() with clear_table=true actually clears the table.
  bpf_tools::BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_HASH(alphabet, char const * const, uint64_t, 26);";
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kProgram));
  auto alphabet = WrappedBCCMap<const char*, uint64_t>::Create(&bcc_wrapper, "alphabet");

  // Set the expected values in the BPF hash table:
  ASSERT_OK(alphabet->SetValue("a", 0));
  ASSERT_OK(alphabet->SetValue("b", 1));
  ASSERT_OK(alphabet->SetValue("c", 2));
  ASSERT_OK(alphabet->SetValue("d", 3));

  using ::testing::IsEmpty;
  using ::testing::Pair;
  using ::testing::UnorderedElementsAre;

  auto gold = UnorderedElementsAre(Pair("a", 0), Pair("b", 1), Pair("c", 2), Pair("d", 3));

  // Verify that get_table_offline() returns the values we expect:
  ASSERT_THAT(alphabet->GetTableOffline(), gold);

  // Verify calling get_table_offline() (again) returns the values we expect,
  // but this time we are passing in parameter clear_table=true:
  constexpr bool kClearTable = true;
  ASSERT_THAT(alphabet->GetTableOffline(kClearTable), gold);

  // After calling get_table_offline() with clear_table=true,
  // the table should be empty.
  ASSERT_THAT(alphabet->GetTableOffline(), IsEmpty());
}

// Tests that BCCWrapper can load and attach UPD filter defined in the XDP program.
TEST(BCCWrapperTest, LoadUPDFilterWithXDP) {
  bpf_tools::BCCWrapper bcc_wrapper;

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

TEST(BCCWrapper, Tracepoint) {
  bpf_tools::BCCWrapper bcc_wrapper;

  // Including sched.h is a workaround of a test failure on 5.18.4 kernel.
  // See https://github.com/iovisor/bcc/issues/4092.
  std::string_view program = R"(
#include <linux/sched.h>
int on_sched_process_exit(struct tracepoint__sched__sched_process_exit* args) {
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t tgid = id >> 32;
    uint32_t pid = id;
    bpf_trace_printk("process_exit: %d %d\n", tgid, pid);
    return 0;
}
  )";

  bpf_tools::TracepointSpec probe_spec{"sched:sched_process_exit", "on_sched_process_exit"};

  ASSERT_OK(bcc_wrapper.InitBPFProgram(program));
  ASSERT_OK(bcc_wrapper.AttachTracepoint(probe_spec));
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
