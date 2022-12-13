/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <istream>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include "src/common/base/base.h"
#include "src/common/system/system.h"

namespace px {
namespace system {

/*
 * ProcParser is use to parse system proc pseudo filesystem.
 */
class ProcParser {
 public:
  ~ProcParser() = default;

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
   * ProcessStatus tracks /proc/<pid>/status.
   */
  struct ProcessStatus {
    int64_t vm_peak_bytes = 0;
    int64_t vm_size_bytes = 0;
    int64_t vm_lck_bytes = 0;
    int64_t vm_pin_bytes = 0;
    int64_t vm_hwm_bytes = 0;
    int64_t vm_rss_bytes = 0;
    int64_t rss_anon_bytes = 0;
    int64_t rss_file_bytes = 0;
    int64_t rss_shmem_bytes = 0;
    int64_t vm_data_bytes = 0;
    int64_t vm_stk_bytes = 0;
    int64_t vm_exe_bytes = 0;
    int64_t vm_lib_bytes = 0;
    int64_t vm_pte_bytes = 0;
    int64_t vm_swap_bytes = 0;
    int64_t hugetlb_pages_bytes = 0;

    int64_t voluntary_ctxt_switches = 0;
    int64_t nonvoluntary_ctxt_switches = 0;
  };

  /**
   * ProcessSMaps tracks memory stats from /proc/<pid>/smaps per address.
   */
  struct ProcessSMaps {
    uint64_t vmem_start = 0;
    uint64_t vmem_end = 0;
    std::string permissions;
    std::string offset;
    std::string pathname;

    int64_t size_bytes = 0;
    int64_t kernel_page_size_bytes = 0;
    int64_t mmu_page_size_bytes = 0;
    int64_t rss_bytes = 0;
    int64_t pss_bytes = 0;
    int64_t shared_clean_bytes = 0;
    int64_t shared_dirty_bytes = 0;
    int64_t private_clean_bytes = 0;
    int64_t private_dirty_bytes = 0;
    int64_t referenced_bytes = 0;
    int64_t anonymous_bytes = 0;
    int64_t lazy_free_bytes = 0;
    int64_t anon_huge_pages_bytes = 0;
    int64_t shmem_pmd_mapped_bytes = 0;
    int64_t file_pmd_mapped_bytes = 0;
    int64_t shared_hugetlb_bytes = 0;
    int64_t private_hugetlb_bytes = 0;
    int64_t swap_bytes = 0;
    int64_t swap_pss_bytes = 0;
    int64_t locked_bytes = 0;

    std::string ToAddress() {
      return absl::Substitute("$0-$1", absl::Hex(this->vmem_start), absl::Hex(this->vmem_end));
    }

    bool operator==(const ProcParser::ProcessSMaps& rhs) const {
      return this->vmem_start == rhs.vmem_start && this->vmem_end == rhs.vmem_end &&
             this->permissions.compare(permissions) == 0 && this->pathname == rhs.pathname;
    }
  };

  /**
   * Parses /proc/<pid>/stat files
   * @param pid is the pid for which we want stat data.
   * @param page_size_bytes The size of memory page in bytes.
   * @param kernel_tick_time_ns The time of each kernel tick in nanoseconds.
   * @param out A valid pointer to the output.
   * @return Status of parsing.
   */
  Status ParseProcPIDStat(int32_t pid, int64_t page_size_bytes, int64_t kernel_tick_time_ns,
                          ProcessStats* out) const;

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
   * Returns the /proc/<pid>/exe
   */
  StatusOr<std::filesystem::path> GetExePath(int32_t pid) const;

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
   * Parses /proc/<pid>/status
   * @param out A valid pointer to the output struct.
   * @return status of parsing
   */
  Status ParseProcPIDStatus(int32_t pid, ProcessStatus* out) const;

  /**
   * Parses /proc/<pid>/smaps
   * @param out A valid pointer to a vector that will contain the output structs.
   * @return status of parsing
   */
  Status ParseProcPIDSMaps(int32_t pid, std::vector<ProcessSMaps>* out) const;

  /**
   * @param pid process id for which to search /proc/<pid>/maps
   * @param out A valid pointer to a vector that will contain the output structs.
   * @return Status containing a hash set of ProcessSMap objects representing all the content in
   * /proc/<pid>/maps
   **/
  Status ParseProcPIDMaps(int32_t pid, std::vector<ProcessSMaps>* out) const;

  /**
   * Parses /proc/<pid>/smaps_rollup for "PSS" the proportional set size memory usage.
   * @return status of parsing or the value of PSS.
   */
  StatusOr<size_t> ParseProcPIDPss(const int32_t pid) const;

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

    std::string ToString() const {
      return absl::Substitute("dev=$0 root=$1 mount_point=$2", dev, root, mount_point);
    }
  };

  Status ReadMountInfos(pid_t pid, std::vector<MountInfo>* mount_infos) const;

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
  StatusOr<absl::flat_hash_set<std::string>> GetMapPaths(pid_t pid) const;

  /**
   * Returns the matching executable memory mapped entry in /prod/<pid>/maps.
   *
   * Since it is possible for multiple entries to exist for the same library,
   * the vmem_start parameter is used to identify a unique entry.
   *
   * For example, given /proc/<pid>/maps:
   * 7f1a50b24000-7f1a50c9c000 r-xp 00022000 fd:00 525817  /usr/lib/x86_64-linux-gnu/libc-2.31.so
   * 7ffff7def000-7ffff7f67000 r-xp 00022000 fd:00 525817  /usr/lib/x86_64-linux-gnu/libc-2.31.so
   *
   * The following function call would return the first entry listed above:
   * GetExecutableMapEntry(<pid>, "/usr/lib/x86_64-linux-gnu/libc-2.31.so", 0x7f1a50b24000)
   **/
  StatusOr<ProcessSMaps> GetExecutableMapEntry(pid_t pid, std::string libpath, uint64_t vmem_start);

 private:
  static Status ParseNetworkStatAccumulateIFaceData(
      const std::vector<std::string_view>& dev_stat_record, NetworkStats* out);

  static void ParseFromKeyValueLine(
      const std::string& line,
      const absl::flat_hash_map<std::string_view, size_t>& field_name_to_value_map,
      uint8_t* out_base);

  static Status ParseFromKeyValueFile(
      const std::string& fpath,
      const absl::flat_hash_map<std::string_view, size_t>& field_name_to_value_map,
      uint8_t* out_base);

  Status ParseProcMapsFile(int32_t pid, std::string filename, std::vector<ProcessSMaps>* out) const;
};

// TODO(jps): Change to GetPIDStartTimeTicks(const pid_t pid), i.e. remove the version that
// uses a filesystem path as an arg.
StatusOr<int64_t> GetPIDStartTimeTicks(const std::filesystem::path& proc_pid_path);

}  // namespace system
}  // namespace px
