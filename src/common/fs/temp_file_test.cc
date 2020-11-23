#include <gtest/gtest.h>

#include <fstream>
#include <string>

#include "src/common/base/file.h"
#include "src/common/base/logging.h"
#include "src/common/base/test_utils.h"
#include "src/common/fs/temp_file.h"

namespace pl {
namespace fs {

TEST(TempFile, Basic) {
  std::unique_ptr<TempFile> tmpf = TempFile::Create();
  std::filesystem::path fpath = tmpf->path();

  ASSERT_OK(WriteFileFromString(fpath, "Some data"));
  ASSERT_OK_AND_EQ(ReadFileToString(fpath), "Some data");
}

TEST(TempFile, ShouldFail) {
  std::filesystem::path fpath;
  {
    std::unique_ptr<TempFile> tmpf = TempFile::Create();
    fpath = tmpf->path();
  }

  ASSERT_NOT_OK(WriteFileFromString(fpath, "Some data"));
}

TEST(TempFile, ReferenceKeepsFileAccessible) {
  std::ifstream fin;
  std::filesystem::path fpath;
  {
    std::unique_ptr<TempFile> tmpf = TempFile::Create();
    fpath = tmpf->path();
    ASSERT_OK(WriteFileFromString(fpath, "Some data"));
    fin = std::ifstream(fpath);
  }

  std::string line;
  std::getline(fin, line);

  EXPECT_EQ(line, "Some data");
}

}  // namespace fs
}  // namespace pl
