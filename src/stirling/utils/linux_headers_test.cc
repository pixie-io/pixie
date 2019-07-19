#include <fstream>

#include "src/stirling/utils/linux_headers.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;

TEST(LinuxHeadersUtils, ParseUnameCases) {
  {
    auto s = ParseUname("4.18.0-25-generic");
    ASSERT_OK(s);
    EXPECT_EQ(266752, s.ConsumeValueOrDie());
  }

  {
    auto s = ParseUname("4.18.0");
    ASSERT_OK(s);
    EXPECT_EQ(266752, s.ConsumeValueOrDie());
  }

  {
    auto s = ParseUname("4.18.");
    EXPECT_NOT_OK(s);
  }

  {
    auto s = ParseUname("linux-4.18.0-25-generic");
    EXPECT_NOT_OK(s);
  }
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

  fs::path base_dir(tmp_dir);
  fs::path version_h_dir = base_dir / "include/generated/uapi/linux";
  fs::create_directories(version_h_dir);
  std::string version_h_filename = version_h_dir / "version.h";

  // Write the original file to disk.
  ASSERT_OK(WriteFileFromString(version_h_filename, version_h_original));

  EXPECT_OK(ModifyKernelVersion(fs::path(base_dir), "4.1.0"));

  // Read the file into a string.
  auto s = ReadFileToString(version_h_filename);
  ASSERT_OK(s);
  std::string file_contents = s.ConsumeValueOrDie();

  std::string expected_contents = R"(#define LINUX_VERSION_CODE 262400
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
)";
  EXPECT_EQ(file_contents, expected_contents);

  fs::remove_all(tmp_dir);
}

}  // namespace stirling
}  // namespace pl
