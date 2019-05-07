#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <experimental/filesystem>

#include <istream>
#include <memory>
#include <sstream>

#include "src/common/system_config/system_config_mock.h"
#include "src/stirling/cgroups/proc_parser.h"

namespace pl {
namespace stirling {

using std::string;
using std::experimental::filesystem::path;
using ::testing::Return;

class ProcParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    common::MockSystemConfig sysconfig;

    EXPECT_CALL(sysconfig, HasSystemConfig()).WillRepeatedly(Return(true));
    EXPECT_CALL(sysconfig, PageSize()).WillRepeatedly(Return(4096));
    EXPECT_CALL(sysconfig, KernelTicksPerSecond()).WillRepeatedly(Return(10000000));

    parser_ = std::make_unique<ProcParser>(sysconfig, proc_path);
    bytes_per_page_ = sysconfig.PageSize();
  }
  const std::string proc_path = "/pl/proc";

  std::unique_ptr<ProcParser> parser_;
  int bytes_per_page_;
};

// TODO(zasgar): refactor into common utils.
string GetTestRunDir() {
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  CHECK(test_src_dir != nullptr);

  return path(test_src_dir) / path("pl/src/stirling/cgroups");
}

// TODO(zasgar): refactor into common utils.
string GetPathToTestDataFile(const string& fname) { return GetTestRunDir() + "/" + fname; }

TEST_F(ProcParserTest, GetProcPidStatFilePath) {
  EXPECT_EQ("/pl/proc/123/stat", parser_->GetProcPidStatFilePath(123));
}

TEST_F(ProcParserTest, GetProcPidStatIOFile) {
  EXPECT_EQ("/pl/proc/123/io", parser_->GetProcPidStatIOFile(123));
}

TEST_F(ProcParserTest, ParseProcPIDNetDev) {
  EXPECT_EQ("/pl/proc/123/net/dev", parser_->GetProcPidNetDevFile(123));
}

TEST_F(ProcParserTest, ParseNetworkStat) {
  ProcParser::NetworkStats stats;
  auto test_file = GetPathToTestDataFile("testdata/proc/sample_net_dev");
  PL_CHECK_OK(parser_->ParseProcPIDNetDev(test_file, &stats));

  // The expeted values are from the test file above.
  EXPECT_EQ(54504114, stats.rx_bytes);
  EXPECT_EQ(65296, stats.rx_packets);
  EXPECT_EQ(0, stats.rx_drops);
  EXPECT_EQ(0, stats.rx_errs);
}

TEST_F(ProcParserTest, ParseStatIO) {
  ProcParser::ProcessStats stats;
  auto test_file = GetPathToTestDataFile("testdata/proc/sample_proc_io");
  PL_CHECK_OK(parser_->ParseProcPIDStatIO(test_file, &stats));

  // The expeted values are from the test file above.
  EXPECT_EQ(5405203, stats.rchar_bytes);
  EXPECT_EQ(1239158, stats.wchar_bytes);
  EXPECT_EQ(17838080, stats.read_bytes);
  EXPECT_EQ(634880, stats.write_bytes);
}

TEST_F(ProcParserTest, ParseStat) {
  ProcParser::ProcessStats stats;
  auto test_file = GetPathToTestDataFile("testdata/proc/sample_proc_stat");
  PL_CHECK_OK(parser_->ParseProcPIDStat(test_file, &stats));

  // The expeted values are from the test file above.
  EXPECT_EQ("ibazel", stats.process_name);

  EXPECT_EQ(800, stats.utime_ns);
  EXPECT_EQ(2300, stats.ktime_ns);
  EXPECT_EQ(13, stats.num_threads);

  EXPECT_EQ(55, stats.major_faults);
  EXPECT_EQ(1799, stats.minor_faults);

  EXPECT_EQ(114384896, stats.vsize_bytes);
  EXPECT_EQ(2577 * bytes_per_page_, stats.rss_bytes);
}

}  // namespace stirling
}  // namespace pl
