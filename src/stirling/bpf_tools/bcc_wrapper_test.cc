#ifdef __linux__

#include "src/stirling/bpf_tools/bcc_wrapper.h"

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::TempDir;

class BCCWraperTest : public ::testing::Test {
 protected:
  static constexpr char kDummyExePath[] = "src/stirling/obj_tools/testdata/dummy_exe";
  static constexpr char kSymbol[] = "CanYouFindThis";
  static constexpr char kBCC[] = R"BCC(
      int foo(struct pt_regs* ctx) {
        return 0;
      }
  )BCC";

  void SetUp() override {
    ASSERT_OK(bcc_wrapper_.InitBPFProgram(kBCC));
    // Copy to temp directory so we can remove it to simulate non-existent file.
    dummy_exe_path_ = temp_dir_.path() / "dummy_exe";
    ASSERT_OK(fs::Copy(BazelBinTestFilePath(kDummyExePath), dummy_exe_path_));
  }

  BCCWrapper bcc_wrapper_;
  TempDir temp_dir_;
  std::filesystem::path dummy_exe_path_;
};

TEST_F(BCCWraperTest, DetachUProbe) {
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
