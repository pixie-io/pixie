#include <gtest/gtest.h>
#include <experimental/filesystem>

#include <algorithm>
#include <istream>
#include <memory>
#include <sstream>
#include <string_view>

#include "src/stirling/cgroups/fs_watcher.h"

namespace pl {
namespace stirling {
namespace fs_watcher {

namespace fs = std::experimental::filesystem;
using std::string;

// TODO(zasgar): refactor into common utils.
string GetTestRunDir() {
  const char *test_src_dir = std::getenv("TEST_SRCDIR");
  const char *test_workspace = std::getenv("TEST_WORKSPACE");
  CHECK(test_src_dir != nullptr);
  CHECK(test_workspace != nullptr);

  return fs::path(test_src_dir) / fs::path("pl/src/stirling/cgroups");
}

// TODO(zasgar): refactor into common utils.
string GetPathToTestDataFile(const string &fname) { return GetTestRunDir() + "/" + fname; }

class FSWatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string prefix = "cgroup_test";
    char dir_template[] = "/tmp/cgroup_test_XXXXXX";
    char *dir_name = mkdtemp(dir_template);
    CHECK(dir_name != nullptr);
    tmp_dir_ = dir_name;
    Test::SetUp();

    fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  }

  void TearDown() override {
    //    fs::remove_all(tmp_dir_);
  }

  FSWatcher fs_watcher_;
  std::string tmp_dir_;
};

TEST_F(FSWatcherTest, fs_watcher_addwatch_removewatch) {
  fs::path besteffort_dir =
      GetPathToTestDataFile("testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/kubepods/besteffort");
  EXPECT_OK(fs_watcher_.AddWatch(besteffort_dir));
  EXPECT_EQ(1, fs_watcher_.NumWatchers());

  fs::path burstable_dir =
      GetPathToTestDataFile("testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/kubepods/burstable");
  EXPECT_OK(fs_watcher_.AddWatch(burstable_dir));
  EXPECT_EQ(2, fs_watcher_.NumWatchers());

  fs::path pod_dir = GetPathToTestDataFile(
      "testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/kubepods/besteffort/"
      "poda22d8c1e-67bf-11e9-b815-42010a8a0135");
  EXPECT_OK(fs_watcher_.AddWatch(pod_dir));
  EXPECT_EQ(3, fs_watcher_.NumWatchers());

  EXPECT_OK(fs_watcher_.RemoveWatch(besteffort_dir));
  EXPECT_EQ(1, fs_watcher_.NumWatchers());

  EXPECT_OK(fs_watcher_.RemoveWatch(burstable_dir));
  EXPECT_EQ(0, fs_watcher_.NumWatchers());
}

TEST_F(FSWatcherTest, fs_watcher_read_inotify_event) {
  // Remove this directory if it already exists.
  fs::remove_all(
      GetPathToTestDataFile("testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/kubepods/"
                            "pod04bfccc8-6526-11e9-b815-42010a8a0135/"
                            "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c9199bff741b5d31e"));

  fs::path procs_file = GetPathToTestDataFile(
      "testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/"
      "kubepods/pod04bfccc8-6526-11e9-b815-42010a8a0135/"
      "43cb2cc960eb486184381495e2f3f3071592f14e9270de586d46ff38b0bd2c2a/"
      "cgroup.procs");

  EXPECT_OK(fs_watcher_.AddWatch(procs_file));

  fs::path pod_dir = GetPathToTestDataFile(
      "testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/"
      "kubepods/pod04bfccc8-6526-11e9-b815-42010a8a0135");

  EXPECT_OK(fs_watcher_.AddWatch(pod_dir));

  EXPECT_FALSE(fs_watcher_.HasEvents());

  // Update procs file.
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/"
                                 "kubepods/pod04bfccc8-6526-11e9-b815-42010a8a0135/"
                                 "43cb2cc960eb486184381495e2f3f3071592f14e9270de586d46ff38b0bd2c2a/"
                                 "cgroup.procs"),
           tmp_dir_, fs::copy_options::overwrite_existing | fs::copy_options::recursive);

  // Add containers
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic_add_pid"),
           GetPathToTestDataFile("testdata/cgroup_basic"),
           fs::copy_options::overwrite_existing | fs::copy_options::recursive);

  EXPECT_EQ(0, fs_watcher_.NumEvents());
  EXPECT_OK(fs_watcher_.ReadInotifyUpdates());
  EXPECT_TRUE(fs_watcher_.HasEvents());

  EXPECT_EQ(2, fs_watcher_.NumEvents());
  auto fs_event = fs_watcher_.GetNextEvent().ConsumeValueOrDie();
  EXPECT_EQ(FSWatcher::FSEventType::kModifyFile, fs_event.type);

  EXPECT_EQ(1, fs_watcher_.NumEvents());
  fs_event = fs_watcher_.GetNextEvent().ConsumeValueOrDie();
  EXPECT_EQ(FSWatcher::FSEventType::kCreateDir, fs_event.type);

  // Reset watchers and FSNode tree. Assuming a rescan of the filesystem.
  EXPECT_OK(fs_watcher_.RemoveAllWatchers());
  EXPECT_EQ(0, fs_watcher_.NumWatchers());

  // Remove the container directory that was created.
  EXPECT_TRUE(fs::remove_all(
      GetPathToTestDataFile("testdata/cgroup_basic/sysfs/cgroup/cpu,cpuacct/kubepods/"
                            "pod04bfccc8-6526-11e9-b815-42010a8a0135/"
                            "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c9199bff741b5d31e")));
}

}  // namespace fs_watcher
}  // namespace stirling
}  // namespace pl
