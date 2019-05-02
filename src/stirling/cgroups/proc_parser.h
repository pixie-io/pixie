#pragma once

#include <experimental/filesystem>
#include <istream>
#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace proc_parser {

// TODO(zasgar): Refactor this to be encapsulated into a class so that we don't
// have to pass around the time/page scaling factors. To make it easily testable
// we will have to abstract away system config into an injectable class.

namespace fs = std::experimental::filesystem;
/**
 * NetworkStats is a struct used to store aggregated network statistics.
 */
struct NetworkStats {
  int64_t rx_bytes = 0;
  int64_t rx_packets = 0;
  int64_t rx_errs = 0;
  int64_t rx_drops = 0;

  int64_t tx_bytes = 0;
  int64_t tx_packets = 0;
  int64_t tx_errs = 0;
  int64_t tx_drops = 0;

  void Clear() { *this = NetworkStats(); }
};

// ProcessStats are basic stats about the process collected from /proc.
// We store all the cpu/memory/io stats together since they belong to a
// single pid and are meant to be consumed together.
struct ProcessStats {
  int64_t pid;
  std::string process_name;

  // Fault information.
  int64_t minor_faults = 0;
  int64_t major_faults = 0;

  // CPU usage & stats.
  int64_t utime_ns = 0;
  int64_t ktime_ns = 0;
  int64_t num_threads = 0;

  // Memory information.
  uint64_t vsize_bytes = 0;
  int64_t rss_bytes = 0;

  // Read/Write to character devices.
  int64_t rchar_bytes = 0;
  int64_t wchar_bytes = 0;

  // Read/Writes to underlying devices.
  int64_t read_bytes = 0;
  int64_t write_bytes = 0;

  void Clear() { *this = ProcessStats(); }
};

fs::path GetProcPidStatFilePath(int64_t pid, fs::path proc_base_path = "/proc");
fs::path GetProcPidStatIOFile(int64_t pid, fs::path proc_base_path = "/proc");
fs::path GetProcPidNetDevFile(int64_t pid, fs::path proc_base_path = "/proc");

/**
 * Parses /proc/<pid>/stat files
 * @param fpath the path to the proc file.
 * @param out A valid pointer to the output.
 * @param bytes_per_page The number of bytes per page used by the OS.
 * @param bytes_per_page The number of bytes in each page of memory.
 * @return Status of parsing.
 */
Status ParseProcPIDStat(const fs::path &fpath, ProcessStats *out, int64_t ns_per_jiffy,
                        int bytes_per_page = 4096);

/**
 * Parses /proc/<pid>/io files.
 * @param fpath the path to the proc file.
 * @param out A valid pointer to an output struct.
 * @return Status of the parsing.
 */
Status ParseProcPIDStatIO(const fs::path &fpath, ProcessStats *out);

/**
 * Parses /proc/<pid>/net/dev
 *
 * It accumulates the results from all network devices into the output. This will ignore virtual,
 * docker and lo interfaces.
 *
 * @param fpath The path to the proc file.
 * @param out A valid pointer to an output struct.
 * @return Status of the parsing.
 */
Status ParseProcPIDNetDev(const fs::path &fpath, NetworkStats *out);

}  // namespace proc_parser
}  // namespace stirling
}  // namespace pl
