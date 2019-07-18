#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <experimental/filesystem>

#include <algorithm>
#include <istream>
#include <memory>
#include <sstream>

#include "src/common/system_config/system_config_mock.h"
#include "src/common/testing/testing.h"
#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;
using std::string;
using ::testing::Return;

constexpr char kTestDataBasePath[] = "src/stirling/cgroups";

namespace {
string GetPathToTestDataFile(const string& fname) {
  return TestEnvironment::PathToTestDataFile(std::string(kTestDataBasePath) + "/" + fname);
}
}  // namespace

class CGroupManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    common::MockSystemConfig sysconfig;

    std::string prefix = "cgroup_test";
    char dir_template[] = "/tmp/cgroup_test_XXXXXX";
    char* dir_name = mkdtemp(dir_template);
    CHECK(dir_name != nullptr);
    tmp_dir_ = dir_name;

    std::string proc = tmp_dir_ + "/proc";
    std::string sysfs = tmp_dir_ + "/sysfs";

    EXPECT_CALL(sysconfig, HasSystemConfig()).WillRepeatedly(Return(true));
    EXPECT_CALL(sysconfig, PageSize()).WillRepeatedly(Return(4096));
    EXPECT_CALL(sysconfig, KernelTicksPerSecond()).WillRepeatedly(Return(10000000));
    EXPECT_CALL(sysconfig, sysfs_path()).WillRepeatedly(Return(sysfs));
    EXPECT_CALL(sysconfig, proc_path()).WillRepeatedly(Return(proc));

    mgr_ = CGroupManager::Create(sysconfig);
  }

  void TearDown() override { fs::remove_all(tmp_dir_); }

  std::unique_ptr<CGroupManager> mgr_;
  std::string tmp_dir_;
};

TEST_F(CGroupManagerTest, cgroup_basic) {
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_TRUE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list = mgr_->PIDsInContainer("pod04bfccc8-6526-11e9-b815-42010a8a0135",
                                         "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c91"
                                         "99bff741b5d31e")
                       .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({123}), *pid_list);

  EXPECT_EQ(1, mgr_->full_scan_count());

  for (const auto& pod_info : mgr_->cgroup_info()) {
    for (const auto& container_info : pod_info.second.container_info_by_name) {
      for (const auto& pid : container_info.second.pids) {
        ProcParser::ProcessStats stats;
        EXPECT_OK(mgr_->GetProcessStats(pid, &stats));
      }
    }
  }
}

TEST_F(CGroupManagerTest, cgroup_basic_add_pid) {
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Add a container.
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic_add_pid"), tmp_dir_,
           fs::copy_options::overwrite_existing | fs::copy_options::recursive);

  // Rescan. For now this should always works since we delete everything, but we
  // can use this to test notification system.
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_TRUE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list = mgr_->PIDsInContainer("pod04bfccc8-6526-11e9-b815-42010a8a0135",
                                         "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c91"
                                         "99bff741b5d31e")
                       .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({123, 789}), *pid_list);

  EXPECT_EQ(2, mgr_->full_scan_count());

  for (const auto& pod_info : mgr_->cgroup_info()) {
    for (const auto& container_info : pod_info.second.container_info_by_name) {
      for (const auto& pid : container_info.second.pids) {
        ProcParser::ProcessStats stats;
        EXPECT_OK(mgr_->GetProcessStats(pid, &stats));
      }
    }
  }
}

TEST_F(CGroupManagerTest, cgroup_empty) {
  // CGroupManager should handle non-existent cgroup directories gracefully.
  // This can happen in real life, if there are not pods in a particular QoS class.
  fs::copy(GetPathToTestDataFile("testdata/cgroup_empty"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Create a bunch of Pods, containers and PIDs.
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_,
           fs::copy_options::overwrite_existing | fs::copy_options::recursive);

  // Rescan. For now this should always works since we delete everything, but we
  // can use this to test notification system.
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_TRUE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list = mgr_->PIDsInContainer("pod04bfccc8-6526-11e9-b815-42010a8a0135",
                                         "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c91"
                                         "99bff741b5d31e")
                       .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({123}), *pid_list);

  EXPECT_EQ(2, mgr_->full_scan_count());

  for (const auto& pod_info : mgr_->cgroup_info()) {
    for (const auto& container_info : pod_info.second.container_info_by_name) {
      for (const auto& pid : container_info.second.pids) {
        ProcParser::ProcessStats stats;
        EXPECT_OK(mgr_->GetProcessStats(pid, &stats));
      }
    }
  }
}

TEST_F(CGroupManagerTest, cgroup_rm_pod) {
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  std::string pod_dir =
      tmp_dir_ + "/sysfs/cgroup/cpu,cpuacct/kubepods/pod04bfccc8-6526-11e9-b815-42010a8a0135";

  uint32_t files_removed;
  files_removed = fs::remove_all(pod_dir);
  ASSERT_EQ(6, files_removed);
  files_removed = fs::remove_all(tmp_dir_ + "/proc/123");
  ASSERT_EQ(5, files_removed);
  files_removed = fs::remove_all(tmp_dir_ + "/proc/456");
  ASSERT_EQ(5, files_removed);

  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_FALSE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list =
      mgr_->PIDsInContainer("poda22d8c1e-67bf-11e9-b815-42010a8a0135",
                            "be03bce2b959975c40cd3796cdb101213aa89b72638b7d46f567ee328863a358")
          .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({234}), *pid_list);

  EXPECT_EQ(2, mgr_->full_scan_count());

  for (const auto& pod_info : mgr_->cgroup_info()) {
    for (const auto& container_info : pod_info.second.container_info_by_name) {
      for (const auto& pid : container_info.second.pids) {
        ProcParser::ProcessStats stats;
        EXPECT_OK(mgr_->GetProcessStats(pid, &stats));
      }
    }
  }
}

TEST_F(CGroupManagerTest, cgroup_rm_container) {
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  std::string container_dir =
      tmp_dir_ +
      "/sysfs/cgroup/cpu,cpuacct/kubepods/pod04bfccc8-6526-11e9-b815-42010a8a0135/"
      "3814823571b7857e7ef48e55414ade5d2d6c0c7d5f62476c9199bff741b5d31e";

  uint32_t files_removed;
  files_removed = fs::remove_all(container_dir);
  ASSERT_EQ(2, files_removed);
  files_removed = fs::remove_all(tmp_dir_ + "/proc/123");
  ASSERT_EQ(5, files_removed);

  PL_CHECK_OK(mgr_->UpdateCGroupInfo());

  // Values are based on the test directory.
  EXPECT_TRUE(mgr_->HasPod("pod04bfccc8-6526-11e9-b815-42010a8a0135"));
  EXPECT_TRUE(mgr_->HasPod("poda22d8c1e-67bf-11e9-b815-42010a8a0135"));
  auto* pid_list =
      mgr_->PIDsInContainer("pod04bfccc8-6526-11e9-b815-42010a8a0135",
                            "43cb2cc960eb486184381495e2f3f3071592f14e9270de586d46ff38b0bd2c2a")
          .ConsumeValueOrDie();
  ASSERT_NE(nullptr, pid_list);
  EXPECT_EQ(std::vector<int64_t>({456}), *pid_list);

  EXPECT_EQ(2, mgr_->full_scan_count());

  for (const auto& pod_info : mgr_->cgroup_info()) {
    for (const auto& container_info : pod_info.second.container_info_by_name) {
      for (const auto& pid : container_info.second.pids) {
        ProcParser::ProcessStats stats;
        EXPECT_OK(mgr_->GetProcessStats(pid, &stats));
      }
    }
  }
}

TEST_F(CGroupManagerTest, network_stats) {
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());
  // Values are based on the test directory.
  std::string pod_name = "pod04bfccc8-6526-11e9-b815-42010a8a0135";
  ASSERT_TRUE(mgr_->HasPod(pod_name));
  ProcParser::NetworkStats stats;

  PL_CHECK_OK(mgr_->GetNetworkStatsForPod(pod_name, &stats));

  // This is well tested in proc_parser, just a quick check to make sure stats
  // are being read.
  EXPECT_EQ(54504114, stats.rx_bytes);
}

TEST_F(CGroupManagerTest, process_stats) {
  fs::copy(GetPathToTestDataFile("testdata/cgroup_basic"), tmp_dir_, fs::copy_options::recursive);
  PL_CHECK_OK(mgr_->UpdateCGroupInfo());
  ProcParser::ProcessStats stats;

  // Values are based on the test directory.
  // The intended use case is to loop through cgroup_info.
  // This is just a quick test to make sure file paths are correct.
  PL_CHECK_OK(mgr_->GetProcessStats(123, &stats));

  // This is well tested in proc_parser, just a quick check to make sure stats
  // are being read and the right multiplication factors for jiffies and pages
  // are being used.
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
