#include <gtest/gtest.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace fs {

TEST(FSWrapperTest, ReadSymlink) {
  const std::string_view kValidSymlinkName = "/tmp/fs_wrapper_test/valid_symlink";
  const std::string_view kBrokenSymlinkName = "/tmp/fs_wrapper_test/broken_symlink";
  const std::string_view kInodeSymlinkName = "/tmp/fs_wrapper_test/inode_symlink";

  EXPECT_OK(CreateSymlinkIfNotExists("/dev/null", kValidSymlinkName));
  EXPECT_OK(CreateSymlinkIfNotExists("/path/to/nowhere", kBrokenSymlinkName));
  EXPECT_OK(CreateSymlinkIfNotExists("socket:[12345]", kInodeSymlinkName));

  EXPECT_OK_AND_EQ(ReadSymlink(kValidSymlinkName), "/dev/null");
  EXPECT_OK_AND_EQ(ReadSymlink(kBrokenSymlinkName), "/path/to/nowhere");
  EXPECT_OK_AND_EQ(ReadSymlink(kInodeSymlinkName), "socket:[12345]");
}

TEST(FSWrapperTest, JoinPath) {
  const std::filesystem::path kAbsPathA = "/path/to/a";
  const std::filesystem::path kAbsPathB = "/path/to/b";
  const std::filesystem::path kRelPathA = "relpath/to/a";
  const std::filesystem::path kRelPathB = "relpath/to/b";

  EXPECT_EQ(JoinPath({&kAbsPathA, &kRelPathA}), "/path/to/a/relpath/to/a");
  EXPECT_EQ(JoinPath({&kAbsPathA, &kRelPathA, &kRelPathB}), "/path/to/a/relpath/to/a/relpath/to/b");
  EXPECT_EQ(JoinPath({&kAbsPathA, &kAbsPathB}), "/path/to/a/path/to/b");
}

}  // namespace fs
}  // namespace pl
