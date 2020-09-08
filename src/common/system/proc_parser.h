#pragma once

#include <filesystem>
#include <istream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include "src/common/base/base.h"
#include "src/common/system/system.h"

namespace pl {
namespace system {

/*
 * ProcParser is use to parse system proc pseudo filesystem.
 */
class ProcParser {
 public:
  ProcParser() = delete;
  ~ProcParser() = default;

  /**
   * ProcParser constructor.
   * @param cfg a reference to the system config. Only needs to be valid for the
   * duration of the constructor call.
   * @param proc_base_path The base path to the proc files.
   */
  explicit ProcParser(const system::Config& cfg);

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
  Status ParseProcPIDStat(int32_t pid, ProcessStats* out) const;

  /**
   * Specialization of ParseProcPIDStat to just extract the start time.
   * @param pid is the pid for which we want the start time.
   * @return start time in unix time since epoch. A time of 0 implies it failed to read the time.
   */
  StatusOr<int64_t> GetPIDStartTimeTicks(int32_t pid) const;

  /**
   * Gets the command line for a given pid.
   * @param pid is the pid for which we want the command line.
   * @return The command line string. Empty string implies we failed to read the file.
   */
  std::string GetPIDCmdline(int32_t pid) const;

  /**
   * Parses /proc/<pid>/io files.
   * @param pid is the pid for which to read IO data.
   * @param out A valid pointer to an output struct.
   * @return Status of the parsing.
   */
  Status ParseProcPIDStatIO(int32_t pid, ProcessStats* out) const;

  /**
   * Parses /proc/<pid>/net/dev
   *
   * It accumulates the results from all network devices into the output. This
   * will ignore virtual, docker and lo interfaces.
   *
   * @param pid is the pid for which we want net data.
   * @param out A valid pointer to an output struct.
   * @return Status of the parsing.
   */
  Status ParseProcPIDNetDev(int32_t pid, NetworkStats* out) const;

  /**
   * Parses /proc/stat
   * @param out a valid pointer to an output struct.
   * @return status of parsing.
   */
  Status ParseProcStat(SystemStats* out) const;

  /**
   * Parses /proc/meminfo
   * @param out A valid pointer to the output struct.
   * @return status of parsing
   */
  Status ParseProcMemInfo(SystemStats* out) const;

  /**
   * Reads and returns the /proc/<pid>/fd/<fd> file descriptor link.
   *
   * Some examples of FD links are below:
   *   socket:[28068]
   *   anon_inode:inotify
   *   anon_inode:[eventpoll]
   *   pipe:[29553]
   *   /path/to/file
   *
   * @param pid is the pid for which we want net data.
   * @param fd is the fd for which we want net data.
   * @param out A valid pointer to an output string.
   * @return Status of the parsing.
   */
  Status ReadProcPIDFDLink(int32_t pid, int32_t fd, std::string* out) const;

  /**
   * UIDs associated with a process.
   */
  struct ProcUIDs {
    std::string real;
    std::string effective;
    // http://man7.org/linux/man-pages/man2/setresgid.2.html
    std::string saved_set;
    // http://man7.org/linux/man-pages/man2/setfsuid.2.html
    std::string filesystem;
  };

  /**
   * Writes 4 UIDs (real, effective, saved set, and filesystem) recorded in /proc/<pid>/status.
   * Returns OK if succeeded.
   */
  Status ReadUIDs(int32_t pid, ProcUIDs* uids) const;

  /**
   * Reads and outputs the namespace pid for a host pid.
   * Returns error::NotFound() if no namespace PID is found.
   * Returns other types of error if failed.
   * http://man7.org/linux/man-pages/man5/proc.5.html
   */
  Status ReadNSPid(pid_t pid, std::vector<std::string>* ns_pids) const;

  /**
   * Represents a record of the proc mountinfo file, like /proc/[pid]/mountinfo.
   * See http://man7.org/linux/man-pages/man5/proc.5.html for more details.
   *
   * Only includes the ones are used by PEM.
   */
  struct MountInfo {
    // The value of st_dev for files on this filesystem.
    std::string dev;
    // The pathname of the directory in the filesystem which forms the root of this mount.
    std::string root;
    // The pathname of the mount point relative to the process's root directory.
    std::string mount_point;
    // The filesystem type in the form "type[.subtype]".
    std::string fs_type;
    // Per-superblock options (see mount(2)).
    std::string options;
  };

  Status ReadMountInfos(pid_t pid, std::vector<MountInfo>* mount_infos) const;

  /**
   * Returns a path to an existent file (or directory) that is mounted to the input mount point
   * of the process specified by the pid. The process could belong to a different mount namespace
   * than the caller.
   */
  StatusOr<std::filesystem::path> ResolveMountPoint(pid_t pid,
                                                    std::filesystem::path mount_point) const;

  /**
   * Returns all mapped paths found in /proc/<pid>/maps.
   *
   * For example, given /proc/<pid>/maps:
   * 56507a582000-56507a5e5000 rw-p 00000000 00:00 0           [heap]
   * ...
   * 7f0be519b000-7f0be51b8000 r--p 00000000 103:02 27147807 /usr/lib/x86_64-linux-gnu/libssl.so.1.1
   *
   * This function would return /usr/lib/x86_64-linux-gnu/libssl.so.1.1 and [heap].
   *
   * Currently, this function is intended for finding .so files used by the the process.
   * It is used by the uprobe deployment on shared libraries, like libssl.so.
   *
   * @param pid Process for which to get mapped paths.
   * @return All map paths, including entries that are not filesystem paths (e.g. [heap]).
   */
  StatusOr<absl::flat_hash_set<std::string>> GetMapPaths(pid_t pid);

 private:
  static Status ParseNetworkStatAccumulateIFaceData(
      const std::vector<std::string_view>& dev_stat_record, NetworkStats* out);

  static Status ParseFromKeyValueFile(
      const std::string& fpath,
      const absl::flat_hash_map<std::string_view, size_t>& field_name_to_value_map,
      uint8_t* out_base, int64_t field_value_multiplier);

  std::filesystem::path ProcPidPath(pid_t pid) const;

  int64_t ns_per_kernel_tick_;
  int32_t bytes_per_page_;

  std::string proc_base_path_;
};

StatusOr<int64_t> GetPIDStartTimeTicks(const std::filesystem::path& proc_pid_path);

}  // namespace system
}  // namespace pl
