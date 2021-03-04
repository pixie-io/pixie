#ifdef __linux__

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

namespace pl {
namespace stirling {
namespace bpf_tools {

using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::TempDir;

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
                       ::pl::system::GetPIDStartTimeTicks("/proc/self"));

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
  ASSERT_EQ(tgid_start_time_output.get_value(0, proc_pid_start_time).code(), 0);

  EXPECT_EQ(proc_pid_start_time, expected_proc_pid_start_time);
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
