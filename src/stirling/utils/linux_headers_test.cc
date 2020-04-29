#include <fstream>

#include "src/common/testing/testing.h"
#include "src/stirling/utils/linux_headers.h"

namespace pl {
namespace stirling {
namespace utils {

bool operator==(const KernelVersion& a, const KernelVersion& b) {
  return (a.version == b.version) && (a.major_rev == b.major_rev) && (a.minor_rev == b.minor_rev);
}

TEST(LinuxHeadersUtils, LinuxVersionCode) {
  KernelVersion version{4, 18, 1};
  EXPECT_EQ(version.code(), 266753);
}

TEST(LinuxHeadersUtils, ParseKernelVersionString) {
  EXPECT_OK_AND_EQ(ParseKernelVersionString("4.18.0-25-generic"), (KernelVersion{4, 18, 0}));
  EXPECT_OK_AND_EQ(ParseKernelVersionString("4.18.4"), (KernelVersion{4, 18, 4}));
  EXPECT_NOT_OK(ParseKernelVersionString("4.18."));
  EXPECT_NOT_OK(ParseKernelVersionString("linux-4.18.0-25-generic"));
}

TEST(LinuxHeadersUtils, GetKernelVersion) {
  StatusOr<KernelVersion> kernel_version_status = GetKernelVersion();
  ASSERT_OK(kernel_version_status);
  KernelVersion kernel_version = kernel_version_status.ValueOrDie();

  // We don't know on what host this test will run, so we don't know what the version code will be.
  // But we can put some bounds, to check for obvious screw-ups.
  // We assume test will run on a Linux machine with kernel 3.x.x or higher,
  // and version 9.x.x or lower.
  // Yes, we're being very generous here, but we want this test to pass the test of time.
  EXPECT_GE(kernel_version.code(), 0x030000);
  EXPECT_LE(kernel_version.code(), 0x090000);
}

TEST(LinuxHeadersUtils, KernelHeadersDistance) {
  // Distances should always be positive.
  EXPECT_GT(KernelHeadersDistance({4, 14, 255}, {4, 15, 10}), 0);

  // Order shouldn't matter.
  EXPECT_EQ(KernelHeadersDistance({4, 14, 255}, {4, 15, 255}),
            KernelHeadersDistance({4, 15, 255}, {4, 14, 255}));

  EXPECT_GT(KernelHeadersDistance({4, 14, 1}, {4, 15, 5}),
            KernelHeadersDistance({4, 14, 1}, {4, 14, 2}));

  // A change in the major version is considered larger than any change in the minor version.
  EXPECT_GT(KernelHeadersDistance({4, 14, 255}, {4, 15, 0}),
            KernelHeadersDistance({4, 14, 255}, {4, 14, 0}));
}

TEST(LinuxHeadersUtils, ModifyVersion) {
  // Test Setup

  // Use 000000 (which is not a real kernel version), so we can detect changes.
  std::string version_h_original = R"(#define LINUX_VERSION_CODE 000000
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
)";

  char tmp_dir_template[] = "/tmp/linux_headers_test_XXXXXX";
  char* tmp_dir = mkdtemp(tmp_dir_template);
  CHECK(tmp_dir != nullptr);

  std::filesystem::path base_dir(tmp_dir);
  std::filesystem::path version_h_dir = base_dir / "include/generated/uapi/linux";
  std::filesystem::create_directories(version_h_dir);
  std::string version_h_filename = version_h_dir / "version.h";

  // Write the original file to disk.
  ASSERT_OK(WriteFileFromString(version_h_filename, version_h_original));

  // Functions Under Test

  StatusOr<KernelVersion> host_linux_version = GetKernelVersion();
  ASSERT_OK(host_linux_version);
  uint32_t host_linux_version_code = host_linux_version.ValueOrDie().code();
  EXPECT_GT(host_linux_version_code, 0);

  EXPECT_OK(ModifyKernelVersion(std::filesystem::path(base_dir), host_linux_version_code));

  // Read the file into a string.
  std::string expected_contents_template =
      "#define LINUX_VERSION_CODE $0\n"
      "#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n";

  std::string expected_contents =
      absl::Substitute(expected_contents_template, host_linux_version_code);

  EXPECT_OK_AND_EQ(ReadFileToString(version_h_filename), expected_contents);

  std::filesystem::remove_all(tmp_dir);
}

TEST(LinuxHeadersUtils, FindClosestPackagedHeader) {
  const std::string kTestSrcDir = testing::TestFilePath("src/stirling/utils/testdata/usr_src");

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path match,
                         FindClosestPackagedHeader(kTestSrcDir, KernelVersion{4, 4, 18}));
    EXPECT_EQ(match.string(), "src/stirling/utils/testdata/usr_src/linux-headers-4.14.176-pl");
  }

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path match,
                         FindClosestPackagedHeader(kTestSrcDir, KernelVersion{4, 15, 10}));
    EXPECT_EQ(match.string(), "src/stirling/utils/testdata/usr_src/linux-headers-4.14.176-pl");
  }

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path match,
                         FindClosestPackagedHeader(kTestSrcDir, KernelVersion{4, 18, 1}));
    EXPECT_EQ(match.string(), "src/stirling/utils/testdata/usr_src/linux-headers-4.18.20-pl");
  }

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path match,
                         FindClosestPackagedHeader(kTestSrcDir, KernelVersion{5, 0, 0}));
    EXPECT_EQ(match.string(), "src/stirling/utils/testdata/usr_src/linux-headers-5.3.18-pl");
  }

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path match,
                         FindClosestPackagedHeader(kTestSrcDir, KernelVersion{5, 7, 20}));
    EXPECT_EQ(match.string(), "src/stirling/utils/testdata/usr_src/linux-headers-5.3.18-pl");
  }
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
