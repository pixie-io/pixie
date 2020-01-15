#include <gtest/gtest.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace fs {

TEST(FSWrapperTest, ReadSymlink) {
  const std::string_view kValidSymlinkName = "/tmp/fs_wrapper_test/valid_symlink";
  const std::string_view kBrokenSymlinkName = "/tmp/fs_wrapper_test/broken_symlink";
  const std::string_view kInodeSymlinkName = "/tmp/fs_wrapper_test/inode_symlink";

  ASSERT_OK(CreateSymlinkIfNotExists("/dev/null", kValidSymlinkName));
  ASSERT_OK(CreateSymlinkIfNotExists("/path/to/nowhere", kBrokenSymlinkName));
  ASSERT_OK(CreateSymlinkIfNotExists("socket:[12345]", kInodeSymlinkName));

  EXPECT_OK_AND_EQ(ReadSymlink(kValidSymlinkName), "/dev/null");
  EXPECT_OK_AND_EQ(ReadSymlink(kBrokenSymlinkName), "/path/to/nowhere");
  EXPECT_OK_AND_EQ(ReadSymlink(kInodeSymlinkName), "socket:[12345]");
}

}  // namespace fs
}  // namespace pl
