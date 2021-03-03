#ifdef __linux__

#include "src/stirling/bpf_tools/bcc_wrapper.h"

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/testdata/dummy_exe_fixture.h"

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

TEST(BCCWrapperTest, InitDefault) {
  // This will look for Linux Headers using the default strategy,
  // but since packaged headers are not included and PL_HOST_ENV is not defined,
  // it essentially boils down to a local headers search.
  // If the test host doesn't have Linux headers, we expect this test to fail.
  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));
}

TEST(BCCWrapperTest, InitWithTaskStructOffsetsResolver) {
  // Force the TaskStructOffsetsResolver to run, so we can test
  // that it doesn't lead to an infinite recursion.
  FLAGS_stirling_always_infer_task_struct_offsets = true;

  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));
}

class BCCWrapperUProbeTest : public ::testing::Test {
 protected:
  static constexpr char kSymbol[] = "CanYouFindThis";

  void SetUp() override {
    ASSERT_OK(bcc_wrapper_.InitBPFProgram(kBCCProgram));

    // Copy dummy_exe to temp directory so we can remove it to simulate non-existent file.
    const obj_tools::DummyExeFixture kDummyExeFixture;
    dummy_exe_path_ = temp_dir_.path() / "dummy_exe";
    ASSERT_OK(fs::Copy(kDummyExeFixture.Path(), dummy_exe_path_));
  }

  BCCWrapper bcc_wrapper_;
  TempDir temp_dir_;
  std::filesystem::path dummy_exe_path_;
};

TEST_F(BCCWrapperUProbeTest, DetachUProbe) {
  UProbeSpec spec = {
      .binary_path = dummy_exe_path_,
      .symbol = kSymbol,
      .probe_fn = "foo",
  };

  {
    ASSERT_OK(bcc_wrapper_.AttachUProbe(spec));
    EXPECT_EQ(1, bcc_wrapper_.num_attached_probes());

    ASSERT_OK(bcc_wrapper_.DetachUProbe(spec));
    EXPECT_EQ(0, bcc_wrapper_.num_attached_probes());
  }

  {
    ASSERT_OK(bcc_wrapper_.AttachUProbe(spec));
    EXPECT_EQ(1, bcc_wrapper_.num_attached_probes());

    // Remove the binary.
    ASSERT_OK(fs::Remove(dummy_exe_path_));
    ASSERT_OK(bcc_wrapper_.DetachUProbe(spec));
    EXPECT_EQ(0, bcc_wrapper_.num_attached_probes());
  }
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
