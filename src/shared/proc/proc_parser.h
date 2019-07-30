#pragma once

#include <istream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system_config/system_config.h"

namespace pl {
namespace stirling {

/*
 * ProcParser is use to parse system proc psuedo filesystem.
 *
 * Note: Methods in here are virtual to make it easy to make.
 */
class ProcParser {
 public:
  ProcParser() = delete;
  virtual ~ProcParser() = default;

  /**
   * ProcParser constructor.
   * @param cfg a reference to the system config. Only needs to be valid for the
   * duration of the constructor call.
   * @param proc_base_path The base path to the proc files.
   */
  explicit ProcParser(const common::SystemConfig& cfg);

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
    int64_t pid = -1;
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

  /**
   * SystemStats tracks system level stats.
   */
  struct SystemStats {
    // CPU stats.
    int64_t cpu_utime_ns = 0;
    int64_t cpu_ktime_ns = 0;

    // Memory stats.
    int64_t mem_total_bytes = 0;
    int64_t mem_free_bytes = 0;
    int64_t mem_available_bytes = 0;
    int64_t mem_buffer_bytes = 0;
    int64_t mem_cached_bytes = 0;
    int64_t mem_swap_cached_bytes = 0;
    int64_t mem_active_bytes = 0;
    int64_t mem_inactive_bytes = 0;

    void Clear() { *this = SystemStats(); }
  };

  /**
   * Parses /proc/<pid>/stat files
   * @param pid is the pid for which we want stat data..
   * @param out A valid pointer to the output.
   * @return Status of parsing.
   */
  virtual Status ParseProcPIDStat(int32_t pid, ProcessStats* out) const;

  /**
   * Specialization of ParseProcPIDStat to just extract the start time.
   * @param pid is the pid for which we want the start time.
   * @return start time in unix time since epoch. A time of 0 implies it failed to read the time.
   */
  virtual int64_t GetPIDStartTime(int32_t pid) const;

  /**
   * Gets the command line for a given pid.
   * @param pid is the pid for which we want the command line.
   * @return The command line string. Empty string implies we failed to read the file.
   */
  virtual std::string GetPIDCmdline(int32_t pid) const;

  /**
   * Parses /proc/<pid>/io files.
   * @param pid is the pid for which to read IO data.
   * @param out A valid pointer to an output struct.
   * @return Status of the parsing.
   */
  virtual Status ParseProcPIDStatIO(int32_t pid, ProcessStats* out) const;

  /**
   * Parses /proc/<pid>/net/dev
   *
   * It accumulates the results from all network devices into the output. This
   * will ignore virtual, docker and lo interfaces.
   *
   * @param pid is the pid for which we want net data..
   * @param out A valid pointer to an output struct.
   * @return Status of the parsing.
   */
  virtual Status ParseProcPIDNetDev(int32_t pid, NetworkStats* out) const;

  /**
   * Parses /proc/stat
   * @param out a valid pointer to an output struct.
   * @return status of parsing.
   */
  virtual Status ParseProcStat(SystemStats* out) const;

  /**
   * Parses /proc/meminfo
   * @param out A valid pointer to the output struct.
   * @return status of parsing
   */
  virtual Status ParseProcMemInfo(SystemStats* out) const;

 private:
  static Status ParseNetworkStatAccumulateIFaceData(
      const std::vector<std::string_view>& dev_stat_record, NetworkStats* out);

  static Status ParseFromKeyValueFile(
      const std::string& fpath,
      const std::unordered_map<std::string_view, size_t>& field_name_to_value_map,
      uint8_t* out_base, int64_t field_value_multiplier);
  int64_t ns_per_kernel_tick_;
  int32_t bytes_per_page_;
  int64_t clock_realtime_offset_;
  std::string proc_base_path_;
};

}  // namespace stirling
}  // namespace pl
