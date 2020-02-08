#include <gtest/gtest.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/temp_dir.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace fs {

using ::testing::HasSubstr;

class FSWrapperTest : public ::testing::Test {
 protected:
  testing::TempDir tmp_dir_;
};

// Tests that CreateDirectories() succeeds even if the directory already exists.
TEST_F(FSWrapperTest, CreateDirectoriesSucceededIfDirectoryAlreadyExists) {
  std::srand(std::time(nullptr));
  const std::filesystem::path target_path = tmp_dir_.path() / "test";
  EXPECT_OK(CreateDirectories(target_path));
  EXPECT_TRUE(std::filesystem::exists(target_path));
  EXPECT_OK(CreateDirectories(target_path));
}

// Tests that CreateSymlink() succeeds.
TEST_F(FSWrapperTest, CreateSymlink) {
  std::srand(std::time(nullptr));
  const std::filesystem::path target_path = tmp_dir_.path() / "test";
  const std::filesystem::path dev_null_path("/dev/null");
  EXPECT_OK(CreateSymlink(dev_null_path, target_path));
  EXPECT_TRUE(error::IsAlreadyExists(CreateSymlink(dev_null_path, target_path)));
}

// Tests that CreateSymlinkIfNotExists() returns error if the requested target is different than the
// already existing symlink.
TEST_F(FSWrapperTest, CreateSymlinkIfNotExists) {
  const std::filesystem::path target_path = tmp_dir_.path() / "test";
  const std::filesystem::path dev_null_path("/dev/null");
  EXPECT_OK(CreateSymlinkIfNotExists(dev_null_path, target_path));
  // Still succeed.
  EXPECT_OK(CreateSymlinkIfNotExists(dev_null_path, target_path));

  const std::filesystem::path dev_null2_path("/dev/null2");
  auto status = CreateSymlinkIfNotExists(dev_null2_path, target_path);
  EXPECT_THAT(status.msg(), HasSubstr("[desired=/dev/null2, actual=/dev/null]"));
}

TEST_F(FSWrapperTest, ReadSymlink) {
  const std::filesystem::path valid_symlink = tmp_dir_.path() / "valid_symlink";
  const std::filesystem::path broken_symlink = tmp_dir_.path() / "broken_symlink";
  const std::filesystem::path inode_symlink = tmp_dir_.path() / "inode_symlink";

  EXPECT_OK(CreateSymlinkIfNotExists("/dev/null", valid_symlink));
  EXPECT_OK(CreateSymlinkIfNotExists("/path/to/nowhere", broken_symlink));
  EXPECT_OK(CreateSymlinkIfNotExists("socket:[12345]", inode_symlink));

  EXPECT_OK_AND_EQ(ReadSymlink(valid_symlink), "/dev/null");
  EXPECT_OK_AND_EQ(ReadSymlink(broken_symlink), "/path/to/nowhere");
  EXPECT_OK_AND_EQ(ReadSymlink(inode_symlink), "socket:[12345]");
}

TEST_F(FSWrapperTest, JoinPath) {
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
