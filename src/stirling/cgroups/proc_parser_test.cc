#include <gtest/gtest.h>
#include <experimental/filesystem>

#include <istream>
#include <sstream>

#include "src/stirling/cgroups/proc_parser.h"

namespace pl {
namespace stirling {

using std::string;
using std::experimental::filesystem::path;

// TODO(zasgar): refactor into common utils.
string GetTestRunDir() {
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  CHECK(test_src_dir != nullptr);

  return path(test_src_dir) / path("pl/src/stirling/cgroups");
}

// TODO(zasgar): refactor into common utils.
string GetPathToTestDataFile(const string& fname) { return GetTestRunDir() + "/" + fname; }

TEST(ProcParser, GetProcPidStatFilePath) {
  EXPECT_EQ("/pl/proc/123/stat", proc_parser::GetProcPidStatFilePath(123, "/pl/proc"));
}

TEST(ProcParser, GetProcPidStatIOFile) {
  EXPECT_EQ("/pl/proc/123/io", proc_parser::GetProcPidStatIOFile(123, "/pl/proc"));
}

TEST(ProcParser, ParseProcPIDNetDev) {
  EXPECT_EQ("/pl/proc/123/net/dev", proc_parser::GetProcPidNetDevFile(123, "/pl/proc"));
}

TEST(ProcParser, ParseNetworkStat) {
  proc_parser::NetworkStats stats;
  auto test_file = GetPathToTestDataFile("testdata/proc/sample_net_dev");
  PL_CHECK_OK(proc_parser::ParseProcPIDNetDev(test_file, &stats));

  // The expeted values are from the test file above.
  EXPECT_EQ(54504114, stats.rx_bytes);
  EXPECT_EQ(65296, stats.rx_packets);
  EXPECT_EQ(0, stats.rx_drops);
  EXPECT_EQ(0, stats.rx_errs);
}

TEST(ProcParser, ParseStatIO) {
  proc_parser::ProcessStats stats;
  auto test_file = GetPathToTestDataFile("testdata/proc/sample_proc_io");
  PL_CHECK_OK(proc_parser::ParseProcPIDStatIO(test_file, &stats));

  // The expeted values are from the test file above.
  EXPECT_EQ(5405203, stats.rchar_bytes);
  EXPECT_EQ(1239158, stats.wchar_bytes);
  EXPECT_EQ(17838080, stats.read_bytes);
  EXPECT_EQ(634880, stats.write_bytes);
}

TEST(ProcParser, ParseStat) {
  proc_parser::ProcessStats stats;
  auto test_file = GetPathToTestDataFile("testdata/proc/sample_proc_stat");
  const int bytes_per_page = 4096;
  const int ns_per_jiffy = 100;
  PL_CHECK_OK(proc_parser::ParseProcPIDStat(test_file, &stats, ns_per_jiffy, bytes_per_page));

  // The expeted values are from the test file above.
  EXPECT_EQ("ibazel", stats.process_name);

  EXPECT_EQ(800, stats.utime_ns);
  EXPECT_EQ(2300, stats.ktime_ns);
  EXPECT_EQ(13, stats.num_threads);

  EXPECT_EQ(55, stats.major_faults);
  EXPECT_EQ(1799, stats.minor_faults);

  EXPECT_EQ(114384896, stats.vsize_bytes);
  EXPECT_EQ(2577 * bytes_per_page, stats.rss_bytes);
}

}  // namespace stirling
}  // namespace pl
