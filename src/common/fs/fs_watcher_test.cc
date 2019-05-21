#include <gtest/gtest.h>
#include <experimental/filesystem>

#include <algorithm>
#include <istream>
#include <memory>
#include <sstream>
#include <string_view>

#include "src/common/fs/fs_watcher.h"
#include "src/common/testing/testing.h"

namespace pl {

namespace fs = std::experimental::filesystem;
using std::string;

constexpr char kTestDataBasePathFS[] = "src/common/fs";

namespace {
string GetPathToTestDataFile(const string &fname) {
  return TestEnvironment::PathToTestDataFile(std::string(kTestDataBasePathFS) + "/" + fname);
}
}  // namespace

class FSWatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string prefix = "fs_watcher_test";
    char dir_template[] = "/tmp/fs_watcher_test_XXXXXX";
    char *dir_name = mkdtemp(dir_template);
    CHECK(dir_name != nullptr);
    tmp_dir_ = dir_name;
    Test::SetUp();

    fs::copy(GetPathToTestDataFile("testdata/fs_watcher"), tmp_dir_, fs::copy_options::recursive);
    fs_watcher_ = FSWatcher::Create();
  }

  void TearDown() override {
    fs_watcher_.reset();
    fs::remove_all(GetPathToTestDataFile("testdata/fs_watcher/dir2/dir3"));
  }

  std::unique_ptr<FSWatcher> fs_watcher_ = nullptr;
  std::string tmp_dir_;
};

TEST_F(FSWatcherTest, fs_watcher_addwatch_removewatch) {
  fs::path dir1 = GetPathToTestDataFile("testdata/fs_watcher/dir1");
  EXPECT_OK(fs_watcher_->AddWatch(dir1));
  EXPECT_EQ(1, fs_watcher_->NumWatchers());

  fs::path dir2 = GetPathToTestDataFile("testdata/fs_watcher/dir2");
  EXPECT_OK(fs_watcher_->AddWatch(dir2));
  EXPECT_EQ(2, fs_watcher_->NumWatchers());

  fs::path file1 = GetPathToTestDataFile("testdata/fs_watcher/dir1/file1.txt");
  EXPECT_OK(fs_watcher_->AddWatch(file1));
  EXPECT_EQ(3, fs_watcher_->NumWatchers());

  EXPECT_OK(fs_watcher_->RemoveWatch(dir1));
  EXPECT_EQ(1, fs_watcher_->NumWatchers());

  EXPECT_OK(fs_watcher_->RemoveWatch(dir2));
  EXPECT_EQ(0, fs_watcher_->NumWatchers());
}

TEST_F(FSWatcherTest, fs_watcher_read_inotify_event) {
  fs::path file1 = GetPathToTestDataFile("testdata/fs_watcher/dir1/file1.txt");

  EXPECT_OK(fs_watcher_->AddWatch(file1));

  fs::path dir2 = GetPathToTestDataFile("testdata/fs_watcher/dir2");

  EXPECT_OK(fs_watcher_->AddWatch(dir2));

  EXPECT_FALSE(fs_watcher_->HasEvents());

  // Update file1.
  fs::copy(GetPathToTestDataFile("testdata/fs_watcher/dir1/file1.txt"), tmp_dir_,
           fs::copy_options::overwrite_existing);

  // Create new dir3 in dir2
  fs::create_directory(GetPathToTestDataFile("testdata/fs_watcher/dir2/dir3"));

  EXPECT_EQ(0, fs_watcher_->NumEvents());
  EXPECT_OK(fs_watcher_->ReadInotifyUpdates());
  EXPECT_TRUE(fs_watcher_->HasEvents());

  // Inotify sometimes reports 2 events for file1. They are both of type
  // kModifyFile with the same event mask.
  // TODO(kgandhi): Uniquify events of the same type for the same file
  // in the inotify event queue.
  EXPECT_LE(2, fs_watcher_->NumEvents());
  auto fs_event = fs_watcher_->GetNextEvent().ConsumeValueOrDie();
  EXPECT_EQ(FSWatcher::FSEventType::kModifyFile, fs_event.type);
  EXPECT_EQ(file1, fs_event.GetPath());

  EXPECT_LE(1, fs_watcher_->NumEvents());
  fs_event = fs_watcher_->GetNextEvent().ConsumeValueOrDie();
  EXPECT_EQ(FSWatcher::FSEventType::kCreateDir, fs_event.type);
  EXPECT_EQ(dir2, fs_event.GetPath());
  EXPECT_EQ("dir3", fs_event.name);
}

}  // namespace pl
