#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"

namespace pl {
namespace md {

constexpr char kTestDataBasePath[] = "src/shared/metadata";

namespace {
std::string GetPathToTestDataFile(const std::string& fname) {
  return TestEnvironment::PathToTestDataFile(std::string(kTestDataBasePath) + "/" + fname);
}
}  // namespace

class CGroupMetadataReaderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    std::string proc = GetPathToTestDataFile("testdata/proc1");
    std::string sysfs = GetPathToTestDataFile("testdata/sysfs1");

    md_reader_.reset(new CGroupMetadataReader(sysfs, proc, 100 /*ns_per_kernel_tick*/,
                                              128 /*clock_realtime_offset*/));
  }

  std::unique_ptr<CGroupMetadataReader> md_reader_;
};

TEST_F(CGroupMetadataReaderTest, read_pid_list) {
  absl::flat_hash_set<uint32_t> pid_set;
  ASSERT_OK(md_reader_->ReadPIDs(PodQOSClass::kBestEffort, "abcd", "c123", &pid_set));
  EXPECT_THAT(pid_set, ::testing::UnorderedElementsAre(123, 456, 789));
}

TEST_F(CGroupMetadataReaderTest, read_pid_start_time) {
  // This is the time from the file * 100 + 128.
  EXPECT_EQ(8001981028, md_reader_->ReadPIDStartTime(32391));
}

TEST_F(CGroupMetadataReaderTest, read_pid_cmdline) {
  EXPECT_THAT("/usr/lib/slack/slack --force-device-scale-factor=1.5 --high-dpi-support=1",
              md_reader_->ReadPIDCmdline(32391));
}

TEST_F(CGroupMetadataReaderTest, read_pid_metadata_null) {
  EXPECT_THAT("/usr/lib/at-spi2-core/at-spi2-registryd --use-gnome-session",
              md_reader_->ReadPIDCmdline(79690));
}

TEST_F(CGroupMetadataReaderTest, cgroup_proc_file_path) {
  EXPECT_EQ(
      "/pl/sys/cgroup/cpu,cpuacct/kubepods/burstable/podabcd/c123/cgroup.procs",
      CGroupMetadataReader::CGroupProcFilePath("/pl/sys", PodQOSClass::kBurstable, "abcd", "c123"));
  EXPECT_EQ("/pl/sys/cgroup/cpu,cpuacct/kubepods/besteffort/podabcd/c123/cgroup.procs",
            CGroupMetadataReader::CGroupProcFilePath("/pl/sys", PodQOSClass::kBestEffort, "abcd",
                                                     "c123"));
  EXPECT_EQ("/pl/sys/cgroup/cpu,cpuacct/kubepods/podabcd/c123/cgroup.procs",
            CGroupMetadataReader::CGroupProcFilePath("/pl/sys", PodQOSClass::kGuaranteed, "abcd",
                                                     "c123"));
}

}  // namespace md
}  // namespace pl
