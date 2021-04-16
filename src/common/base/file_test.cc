#include <filesystem>

#include <absl/strings/str_cat.h>
#include <gtest/gtest.h>

#include "src/common/base/file.h"
#include "src/common/testing/testing.h"

namespace px {

using ::px::testing::TempDir;

TEST(FileUtils, WriteThenRead) {
  std::string write_val = R"(This is a a file content.
It has two lines.)";

  TempDir tmp_dir;
  std::filesystem::path test_file = tmp_dir.path() / "file";

  EXPECT_OK(WriteFileFromString(test_file, write_val));
  std::string read_val = FileContentsOrDie(test_file);
  EXPECT_EQ(read_val, write_val);
}

}  // namespace px
