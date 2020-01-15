#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <istream>
#include <memory>
#include <sstream>
#include <string_view>

#include "src/common/fs/fs_watcher.h"
#include "src/common/testing/testing.h"

namespace pl {

using std::string;

constexpr char kTestDataBasePathFS[] = "src/common/fs";

namespace {
string GetPathToTestDataFile(const string& fname) {
  return TestEnvironment::PathToTestDataFile(std::string(kTestDataBasePathFS) + "/" + fname);
}
}  // namespace

class FSWatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string prefix = "fs_watcher_test";
    char dir_template[] = "/tmp/fs_watcher_test_XXXXXX";
    char* dir_name = mkdtemp(dir_template);
    CHECK(dir_name != nullptr);
    tmp_dir_ = dir_name;
    Test::SetUp();

    std::filesystem::copy(GetPathToTestDataFile("testdata/fs_watcher"), tmp_dir_,
                          std::filesystem::copy_options::recursive);
    fs_watcher_ = FSWatcher::Create();
  }

  void TearDown() override {
    fs_watcher_.reset();
    std::filesystem::remove_all(tmp_dir_);
  }

  std::unique_ptr<FSWatcher> fs_watcher_ = nullptr;
  std::string tmp_dir_;
};

TEST_F(FSWatcherTest, fs_watcher_addwatch_removewatch) {
  std::filesystem::path dir1 = tmp_dir_ + "/dir1";
  EXPECT_OK(fs_watcher_->AddWatch(dir1));
  EXPECT_EQ(1, fs_watcher_->NumWatchers());

  std::filesystem::path dir2 = tmp_dir_ + "/dir2";
  EXPECT_OK(fs_watcher_->AddWatch(dir2));
  EXPECT_EQ(2, fs_watcher_->NumWatchers());

  std::filesystem::path file1 = tmp_dir_ + "/dir1/file1.txt";
  EXPECT_OK(fs_watcher_->AddWatch(file1));
  EXPECT_EQ(3, fs_watcher_->NumWatchers());

  EXPECT_OK(fs_watcher_->RemoveWatch(dir1));
  EXPECT_EQ(1, fs_watcher_->NumWatchers());

  EXPECT_OK(fs_watcher_->RemoveWatch(dir2));
  EXPECT_EQ(0, fs_watcher_->NumWatchers());
}

TEST_F(FSWatcherTest, fs_watcher_read_inotify_event) {
  std::filesystem::path file1 = tmp_dir_ + "/dir1/file1.txt";
  EXPECT_OK(fs_watcher_->AddWatch(file1));

  std::filesystem::path dir2 = tmp_dir_ + "/dir2";
  EXPECT_OK(fs_watcher_->AddWatch(dir2));

  EXPECT_FALSE(fs_watcher_->HasEvents());

  // Modify file1.
  std::filesystem::copy(tmp_dir_ + "/dir2/file2.txt", tmp_dir_ + "/dir1/file1.txt",
                        std::filesystem::copy_options::overwrite_existing);

  // Create new dir3 in dir2.
  std::filesystem::create_directory(tmp_dir_ + "/dir2/dir3");

  EXPECT_EQ(0, fs_watcher_->NumEvents());
  EXPECT_OK(fs_watcher_->ReadInotifyUpdates());
  EXPECT_TRUE(fs_watcher_->HasEvents());

  // Inotify sometimes reports 2 events for file1. They are both of type
  // kModifyFile with the same event mask.
  // TODO(kgandhi): Uniquify events of the same type for the same file
  // in the inotify event queue.
  EXPECT_EQ(2, fs_watcher_->NumEvents());
  auto fs_event = fs_watcher_->GetNextEvent().ConsumeValueOrDie();
  EXPECT_EQ(FSWatcher::FSEventType::kModifyFile, fs_event.type);
  EXPECT_EQ(file1, fs_event.GetPath());

  EXPECT_EQ(1, fs_watcher_->NumEvents());
  fs_event = fs_watcher_->GetNextEvent().ConsumeValueOrDie();
  EXPECT_EQ(FSWatcher::FSEventType::kCreateDir, fs_event.type);
  EXPECT_EQ(dir2, fs_event.GetPath());
  EXPECT_EQ("dir3", fs_event.name);
}

}  // namespace pl
