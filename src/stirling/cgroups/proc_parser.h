#pragma once

#include <experimental/filesystem>
#include <istream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system_config/system_config.h"

namespace pl {
namespace stirling {

namespace fs = std::experimental::filesystem;

class ProcParser {
 public:
  ProcParser() = delete;

  /**
   * ProcParser constructor.
   * @param cfg a reference to the system config. Only needs to be valid for the duration of the
   * constructor call.
   * @param proc_base_path The base path to the proc files.
   */
  ProcParser(const common::SystemConfig &cfg, std::string_view proc_base_path)
      : proc_base_path_(proc_base_path) {
    CHECK(cfg.HasSystemConfig()) << "System config is required for the ProcParser";
    ns_per_kernel_tick_ = static_cast<int64_t>(1E9 / cfg.KernelTicksPerSecond());
    bytes_per_page_ = cfg.PageSize();
  }

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

  fs::path GetProcStatFilePath();
  fs::path GetProcPidStatFilePath(int64_t pid);
  fs::path GetProcPidStatIOFile(int64_t pid);
  fs::path GetProcPidNetDevFile(int64_t pid);

  /**
   * Parses /proc/<pid>/stat files
   * @param fpath the path to the proc file.
   * @param out A valid pointer to the output.
   * @return Status of parsing.
   */
  Status ParseProcPIDStat(const fs::path &fpath, ProcessStats *out);

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
  static Status ParseProcPIDNetDev(const fs::path &fpath, NetworkStats *out);

  /**
   * Parses /proc/stat
   * @param fpath The path to the proc stat file.
   * @param out a valid pointer to an output struct.
   * @return status of parsing.
   */
  static Status ParseProcStat(const fs::path &fpath, SystemStats *out);

  /**
   * Parses /proc/meminfo
   * @param fpath the path to the proc file.
   * @param out A valid pointer to the output struct.
   * @return status of parsing
   */
  Status ParseProcMemInfo(const fs::path &fpath, SystemStats *out);

 private:
  static Status ParseNetworkStatAccumulateIFaceData(
      const std::vector<std::string_view> &dev_stat_record, NetworkStats *out);

  static Status ParseFromKeyValueFile(
      const fs::path &fpath,
      const std::unordered_map<std::string_view, int64_t *> &field_name_to_value_map,
      int64_t field_value_multiplier);
  int64_t ns_per_kernel_tick_;
  int32_t bytes_per_page_;
  fs::path proc_base_path_;
};

}  // namespace stirling
}  // namespace pl
