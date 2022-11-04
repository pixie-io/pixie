/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"

namespace px {
namespace fs {

using ::px::testing::status::StatusIs;
using ::testing::ElementsAre;
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
  const std::filesystem::path kEmpty;
  const std::filesystem::path kRoot = "/";
  const std::filesystem::path kAbsPathA = "/path/to/a";
  const std::filesystem::path kAbsPathB = "/path/to/b";
  const std::filesystem::path kRelPathA = "relpath/to/a";
  const std::filesystem::path kRelPathB = "relpath/to/b";

  EXPECT_EQ(JoinPath({&kEmpty, &kAbsPathA}), "/path/to/a");
  EXPECT_EQ(JoinPath({&kEmpty, &kRelPathA}), "relpath/to/a");
  EXPECT_EQ(JoinPath({&kRoot, &kEmpty, &kAbsPathA}), "/path/to/a");
  EXPECT_EQ(JoinPath({&kRoot, &kEmpty, &kRelPathA}), "/relpath/to/a");
  EXPECT_EQ(JoinPath({&kAbsPathA, &kEmpty}), "/path/to/a");
  EXPECT_EQ(JoinPath({&kRelPathA, &kEmpty}), "relpath/to/a");

  EXPECT_EQ(JoinPath({&kRoot, &kAbsPathA}), "/path/to/a");
  EXPECT_EQ(JoinPath({&kRoot, &kRelPathA}), "/relpath/to/a");

  EXPECT_EQ(JoinPath({&kAbsPathA, &kRelPathA}), "/path/to/a/relpath/to/a");
  EXPECT_EQ(JoinPath({&kAbsPathA, &kRelPathA, &kRelPathB}), "/path/to/a/relpath/to/a/relpath/to/b");
  EXPECT_EQ(JoinPath({&kAbsPathA, &kAbsPathB}), "/path/to/a/path/to/b");
}

TEST_F(FSWrapperTest, ExistsReturnsErrorForNonExistentFile) {
  EXPECT_FALSE(Exists(tmp_dir_.path() / "non-existent"));
}

TEST_F(FSWrapperTest, GetChildRelPath) {
  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b", "/a/b"), "");
  EXPECT_OK_AND_EQ(GetChildRelPath("a/b", "a/b"), "");
  EXPECT_OK_AND_EQ(GetChildRelPath("/", "/"), "");

  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b/c", "/a/b"), "c");
  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b/c", "/a"), "b/c");
  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b/c", "/"), "a/b/c");

  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b/c/", "/a/b"), "c/");
  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b/c/", "/a"), "b/c/");
  EXPECT_OK_AND_EQ(GetChildRelPath("/a/b/c/", "/"), "a/b/c/");

  EXPECT_OK_AND_EQ(GetChildRelPath("a/b/c", "a/b"), "c");
  EXPECT_OK_AND_EQ(GetChildRelPath("a/b/c", "a"), "b/c");
  EXPECT_OK_AND_EQ(GetChildRelPath("a/b/c/", "a/b"), "c/");
  EXPECT_OK_AND_EQ(GetChildRelPath("a/b/c/", "a"), "b/c/");

  EXPECT_NOT_OK(GetChildRelPath("/aaa/b", "/a"));
  EXPECT_NOT_OK(GetChildRelPath("aaa/b", "a"));

  EXPECT_NOT_OK(GetChildRelPath("a/b", "/a/b"));
  EXPECT_NOT_OK(GetChildRelPath("/a/b", "a/b"));

  // Paths with trailing slash are not working.
  EXPECT_NOT_OK(GetChildRelPath("/a/b", "/a/"));
  EXPECT_NOT_OK(GetChildRelPath("a/b", "a/"));

  // The following behaviors might be confusing.
  // But it's not an issue for our use cases.
  EXPECT_NOT_OK(GetChildRelPath("", "a/b/c"));
  EXPECT_NOT_OK(GetChildRelPath("/a/b/c", ""));
  EXPECT_NOT_OK(GetChildRelPath(".", "a/b/c"));
}

bool operator==(const PathSplit& lhs, const PathSplit& rhs) {
  return lhs.parent == rhs.parent && lhs.child == rhs.child;
}

TEST_F(FSWrapperTest, EnumerateParentPaths) {
  EXPECT_THAT(EnumerateParentPaths("a/b/c/d"),
              ElementsAre(PathSplit{"a/b/c/d", ""}, PathSplit{"a/b/c", "d"},
                          PathSplit{"a/b", "c/d"}, PathSplit{"a", "b/c/d"}));
  EXPECT_THAT(
      EnumerateParentPaths("/a/b/c/d"),
      ElementsAre(PathSplit{"/a/b/c/d", ""}, PathSplit{"/a/b/c", "d"}, PathSplit{"/a/b", "c/d"},
                  PathSplit{"/a", "b/c/d"}, PathSplit{"/", "a/b/c/d"}));
}

// Tests that SpaceAvailableInBytes succeeds and returns a non-zero value.
TEST_F(FSWrapperTest, SpaceAvailableInBytes) {
  auto s = SpaceAvailableInBytes(tmp_dir_.path());
  EXPECT_OK(s);
  const int64_t space_available = s.ConsumeValueOrDie();
  EXPECT_GT(space_available, 0);
}

// Tests that SpaceAvailableInBytes returns an error for a non-existent path.
TEST_F(FSWrapperTest, SpaceAvailableInBytesError) {
  const std::filesystem::path fake_path = tmp_dir_.path() / "non-existent";
  EXPECT_NOT_OK(SpaceAvailableInBytes(fake_path));
}

}  // namespace fs
}  // namespace px
