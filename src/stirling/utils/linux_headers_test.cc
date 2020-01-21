#include <fstream>

#include "src/common/testing/testing.h"
#include "src/stirling/utils/linux_headers.h"

namespace pl {
namespace stirling {
namespace utils {

TEST(LinuxHeadersUtils, ParseUnameCases) {
  EXPECT_OK_AND_EQ(ParseUname("4.18.0-25-generic"), 266752);
  EXPECT_OK_AND_EQ(ParseUname("4.18.0"), 266752);
  EXPECT_NOT_OK(ParseUname("4.18."));
  EXPECT_NOT_OK(ParseUname("linux-4.18.0-25-generic"));
}

TEST(LinuxHeadersUtils, ModifyVersion) {
  std::string version_h_original = R"(#define LINUX_VERSION_CODE 260000
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
)";

  std::string version_h_modified = R"(#define LINUX_VERSION_CODE 262400
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

  EXPECT_OK(ModifyKernelVersion(std::filesystem::path(base_dir), "4.1.0"));

  // Read the file into a string.
  std::string expected_contents = R"(#define LINUX_VERSION_CODE 262400
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
)";
  EXPECT_OK_AND_EQ(ReadFileToString(version_h_filename), expected_contents);

  std::filesystem::remove_all(tmp_dir);
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
