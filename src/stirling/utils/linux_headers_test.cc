#include <fstream>

#include "src/common/testing/testing.h"
#include "src/stirling/utils/linux_headers.h"

namespace pl {
namespace stirling {
namespace utils {

TEST(LinuxHeadersUtils, VersionStringToCode) {
  EXPECT_OK_AND_EQ(VersionStringToCode("4.18.0-25-generic"), 266752);
  EXPECT_OK_AND_EQ(VersionStringToCode("4.18.0"), 266752);
  EXPECT_NOT_OK(VersionStringToCode("4.18."));
  EXPECT_NOT_OK(VersionStringToCode("linux-4.18.0-25-generic"));
}

TEST(LinuxHeadersUtils, LinuxVersionCode) {
  // We don't know on what host this test will run, so we don't know what the version code will be.
  // But we can put some bounds, to check for obvious screw-ups.
  // We assume test will run on a Linux machine with kernel 3.x.x or higher,
  // and version 9.x.x or lower.
  // Yes, we're being very generous here, but we want this test to pass the test of time.
  EXPECT_OK_AND_GE(LinuxVersionCode(), 0x030000);
  EXPECT_OK_AND_LE(LinuxVersionCode(), 0x090000);
}

TEST(LinuxHeadersUtils, ModifyVersion) {
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

  EXPECT_OK(ModifyKernelVersion(std::filesystem::path(base_dir)));

  StatusOr<uint32_t> host_linux_version = LinuxVersionCode();
  EXPECT_OK_AND_GE(host_linux_version, 0);

  // Read the file into a string.
  std::string expected_contents_template =
      "#define LINUX_VERSION_CODE $0\n"
      "#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n";

  std::string expected_contents =
      absl::Substitute(expected_contents_template, host_linux_version.ValueOrDie());

  EXPECT_OK_AND_EQ(ReadFileToString(version_h_filename), expected_contents);

  std::filesystem::remove_all(tmp_dir);
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
