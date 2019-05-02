#include <gtest/gtest.h>
#include <experimental/filesystem>

#include <algorithm>
#include <istream>
#include <memory>
#include <sstream>

#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;
using std::string;

// TODO(zasgar): refactor into common utils.
string GetTestRunDir() {
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  const char* test_workspace = std::getenv("TEST_WORKSPACE");
  CHECK(test_src_dir != nullptr);
  CHECK(test_workspace != nullptr);

  return fs::path(test_src_dir) / fs::path("pl/src/stirling/cgroups");
}

// TODO(zasgar): refactor into common utils.
string GetPathToTestDataFile(const string& fname) { return GetTestRunDir() + "/" + fname; }

class CGroupManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string prefix = "cgroup_test";
    char dir_template[] = "/tmp/cgroup_test_XXXXXX";
    char* dir_name = mkdtemp(dir_template);
    CHECK(dir_name != nullptr);
    tmp_dir_ = dir_name;
    Test::SetUp();

    int bytes_per_page = 4096;
    int64_t ns_per_jiffy = 100;

    std::string proc = tmp_dir_ + "/proc";
    std::string sysfs = tmp_dir_ + "/sysfs";
    fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
    mgr_ = CGroupManager::Create(proc, sysfs, bytes_per_page, ns_per_jiffy);
  }

  void TearDown() override {
    //    fs::remove_all(tmp_dir_);
  }

  std::unique_ptr<CGroupManager> mgr_;
  std::string tmp_dir_;
};

TEST_F(CGroupManagerTest, cgroup_basic) {
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_TRUE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list =
      mgr_->PIDsInContainer("pod04bfccc8-6526-11e9-b815-42010a8a0135",
                            "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c9199bff741b5d31e")
          .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({123}), *pid_list);
}

TEST_F(CGroupManagerTest, cgroup_basic_add_pid) {
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Add a container.
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic_add_pid"), tmp_dir_,
           fs::copy_options::overwrite_existing | fs::copy_options::recursive);

  // Rescan. For now this should always works since we delete everything, but we can use this to
  // test notification system.
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_TRUE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list =
      mgr_->PIDsInContainer("pod04bfccc8-6526-11e9-b815-42010a8a0135",
                            "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c9199bff741b5d31e")
          .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({123, 789}), *pid_list);
}

TEST_F(CGroupManagerTest, network_stats) {
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());
  // Values are based on the test directory.
  std::string pod_name = "pod04bfccc8-6526-11e9-b815-42010a8a0135";
  ASSERT_TRUE(mgr_->HasPod(pod_name));
  proc_parser::NetworkStats stats;

  PL_CHECK_OK(mgr_->GetNetworkStatsForPod(pod_name, &stats));

  // This is well tested in proc_parser, just a quick check to make sure stats are being read.
  EXPECT_EQ(54504114, stats.rx_bytes);
}

TEST_F(CGroupManagerTest, process_stats) {
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());
  proc_parser::ProcessStats stats;

  // Values are based on the test directory.
  // The intended use case is to loop through cgroup_info.
  // This is just a quick test to make sure file paths are correct.
  PL_CHECK_OK(mgr_->GetProcessStats(123, &stats));

  // This is well tested in proc_parser, just a quick check to make sure stats are being read
  // and the right multiplication factors for jiffies and pages are being used.
  EXPECT_EQ("ibazel", stats.process_name);
  EXPECT_EQ(800, stats.utime_ns);
  EXPECT_EQ(10555392, stats.rss_bytes);
  EXPECT_EQ(17838080, stats.read_bytes);
}

TEST(CGroupQoS, ToString) {
  using qos = CGroupManager::CGroupQoS;

  EXPECT_EQ("BestEffort", CGroupManager::CGroupQoSToString(qos::kBestEffort));
  EXPECT_EQ("Burstable", CGroupManager::CGroupQoSToString(qos::kBurstable));
  EXPECT_EQ("Guaranteed", CGroupManager::CGroupQoSToString(qos::kGuaranteed));
}

}  // namespace stirling
}  // namespace pl
