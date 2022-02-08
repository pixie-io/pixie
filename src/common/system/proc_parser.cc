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

#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/numbers.h>
#include <absl/strings/substitute.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_parser.h"

namespace px {
namespace system {

// Separators of the fields in the various files in the proc filesystem.
constexpr char kFieldSeparators[] = "\t ";

/**
 * Only local network interfaces with these prefixes are included in rx/tx computations.
 * See for conventional prefixes:
 *  https://www.freedesktop.org/wiki/Software/systemd/PredictableNetworkInterfaceNames/
 *  https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/8/html/configuring_and_managing_networking/consistent-network-interface-device-naming_configuring-and-managing-networking
 */
const std::vector<std::string> kNetIFacePrefix = {
    // Ethernet interfaces. "en" covers ens, eno, enx, enp.
    "eth",
    "en",
    // Wireless interfaces.
    "wlan",
    "wl",
    "ww",
};

/*************************************************
 * Constants for the /proc/stat file
 *************************************************/
constexpr int kProcStatCPUNumFields = 11;
constexpr int KProcStatCPUUTimeField = 1;
constexpr int KProcStatCPUKTimeField = 3;

/*************************************************
 * Constants for the /proc/<pid>/net/dev file
 *************************************************/
constexpr int kProcNetDevNumFields = 17;

constexpr int kProcNetDevIFaceField = 0;
constexpr int kProcNetDevRxBytesField = 1;
constexpr int kProcNetDevRxPacketsField = 2;
constexpr int kProcNetDevRxErrsField = 3;
constexpr int kProcNetDevRxDropField = 4;

constexpr int kProcNetDevTxBytesField = 9;
constexpr int kProcNetDevTxPacketsField = 10;
constexpr int kProcNetDevTxErrsField = 11;
constexpr int kProcNetDevTxDropField = 12;

/*************************************************
 * constants for the /proc/<pid>/stat file
 *************************************************/
constexpr int kProcStatNumFields = 52;

constexpr int kProcStatPIDField = 0;
constexpr int kProcStatProcessNameField = 1;

constexpr int kProcStatMinorFaultsField = 9;
constexpr int kProcStatMajorFaultsField = 11;

constexpr int kProcStatUTimeField = 13;
constexpr int kProcStatKTimeField = 14;
constexpr int kProcStatNumThreadsField = 19;

constexpr int kProcStatStartTimeField = 21;

constexpr int kProcStatVSizeField = 22;
constexpr int kProcStatRSSField = 23;

std::filesystem::path ProcParser::ProcPidPath(pid_t pid) const {
  return std::filesystem::path(proc_base_path_) / std::to_string(pid);
}

ProcParser::ProcParser(const system::Config& cfg) {
  CHECK(cfg.HasConfig()) << "System config is required for the ProcParser";
  ns_per_kernel_tick_ = static_cast<int64_t>(1E9 / cfg.KernelTicksPerSecond());
  bytes_per_page_ = cfg.PageSize();
  proc_base_path_ = cfg.proc_path();
}

Status ProcParser::ParseNetworkStatAccumulateIFaceData(
    const std::vector<std::string_view>& dev_stat_record, NetworkStats* out) {
  DCHECK(out != nullptr);

  int64_t val;
  bool ok = true;
  // Rx Data.
  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevRxBytesField], &val);
  out->rx_bytes += val;

  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevRxPacketsField], &val);
  out->rx_packets += val;

  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevRxDropField], &val);
  out->rx_drops += val;

  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevRxErrsField], &val);
  out->rx_errs += val;

  // Tx Data.
  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevTxBytesField], &val);
  out->tx_bytes += val;

  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevTxPacketsField], &val);
  out->tx_packets += val;

  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevTxDropField], &val);
  out->tx_drops += val;

  ok &= absl::SimpleAtoi(dev_stat_record[kProcNetDevTxErrsField], &val);
  out->tx_errs += val;

  if (!ok) {
    // This should never happen since it requires the file to be ill-formed
    // by the kernel.
    return error::Internal("failed to parse net dev file");
  }

  return Status::OK();
}

bool ShouldIncludeNetIFace(const std::string_view iface) {
  // TODO(oazizi): Need a better way to know which interfaces to include.
  for (const auto& prefix : kNetIFacePrefix) {
    if (absl::StartsWith(iface, prefix)) {
      return true;
    }
  }
  return false;
}

Status ProcParser::ParseProcPIDNetDev(int32_t pid, NetworkStats* out) const {
  /**
   * Sample file:
   * Inter-|   Receive                                                | Transmit
   * face |bytes    packets errs drop fifo frame compressed multicast|bytes
   * packets errs drop fifo colls carrier compressed ens33: 54504114   65296 0
   * 0    0     0          0         0 4258632   39739    0    0    0     0 0 0
   * vnet1: 3936114   23029    0    0    0 0          0         0 551949355
   * 42771    0    0    0     0       0          0
   *
   */
  DCHECK(out != nullptr);

  std::string fpath = absl::Substitute("$0/$1/net/dev", proc_base_path_, pid);
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
  }

  // Ignore the first two lines since they are just headers;
  const int kHeaderLines = 2;
  for (int i = 0; i < kHeaderLines; ++i) {
    ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }

  std::string line;
  while (std::getline(ifs, line)) {
    std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
    // We check less than in case more fields are added later.
    if (split.size() < kProcNetDevNumFields) {
      return error::Internal("failed to parse net dev file, incorrect number of fields");
    }

    if (!ShouldIncludeNetIFace(split[kProcNetDevIFaceField])) {
      continue;
    }

    // We should track this interface. Accumulate the results.
    auto s = ParseNetworkStatAccumulateIFaceData(split, out);
    if (!s.ok()) {
      // Empty out the stats so we don't leave intermediate results.
      return s;
    }
  }

  return Status::OK();
}

Status ProcParser::ParseProcPIDStat(int32_t pid, ProcessStats* out) const {
  /**
   * Sample file:
   * 4602 (ibazel) S 3260 4602 3260 34818 4602 1077936128 1799 174589 \
   * 55 68 8 23 106 72 20 0 13 0 14329 114384896 2577 18446744073709551615 \
   * 4194304 7917379 140730842479232 0 0 0 1006254592 0 2143420159 0 0 0 17 \
   * 3 0 0 3 0 0 12193792 12432192 34951168 140730842488151 140730842488200 \
   * 140730842488200 140730842492896 0
   */
  DCHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/$1/stat", proc_base_path_, pid);

  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
  }

  std::string line;
  bool ok = true;
  if (std::getline(ifs, line)) {
    std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
    // We check less than in case more fields are added later.
    if (split.size() < kProcStatNumFields) {
      return error::Unknown("Incorrect number of fields in stat file: $0", fpath);
    }
    ok &= absl::SimpleAtoi(split[kProcStatPIDField], &out->pid);
    // The name is surrounded by () we remove it here.
    const std::string_view& name_field = split[kProcStatProcessNameField];
    if (name_field.length() > 2) {
      out->process_name = std::string(name_field.substr(1, name_field.size() - 2));
    } else {
      ok = false;
    }
    ok &= absl::SimpleAtoi(split[kProcStatMinorFaultsField], &out->minor_faults);
    ok &= absl::SimpleAtoi(split[kProcStatMajorFaultsField], &out->major_faults);

    ok &= absl::SimpleAtoi(split[kProcStatUTimeField], &out->utime_ns);
    ok &= absl::SimpleAtoi(split[kProcStatKTimeField], &out->ktime_ns);
    // The kernel tracks utime and ktime in kernel ticks.
    out->utime_ns *= ns_per_kernel_tick_;
    out->ktime_ns *= ns_per_kernel_tick_;

    ok &= absl::SimpleAtoi(split[kProcStatNumThreadsField], &out->num_threads);
    ok &= absl::SimpleAtoi(split[kProcStatVSizeField], &out->vsize_bytes);
    ok &= absl::SimpleAtoi(std::string(split[kProcStatRSSField]), &out->rss_bytes);

    // RSS is in pages.
    out->rss_bytes *= bytes_per_page_;

  } else {
    return error::Internal("Failed to read proc stat file: $0", fpath);
  }

  if (!ok) {
    // This should never happen since it requires the file to be ill-formed
    // by the kernel.
    return error::Internal("failed to parse stat file ($0). ATOI failed.", fpath);
  }
  return Status::OK();
}

Status ProcParser::ParseProcPIDStatIO(int32_t pid, ProcessStats* out) const {
  /**
   * Sample file:
   *   rchar: 5405203
   *   wchar: 1239158
   *   syscr: 10608
   *   syscw: 3141
   *   read_bytes: 17838080
   *   write_bytes: 634880
   *   cancelled_write_bytes: 192512
   */
  DCHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/$1/io", proc_base_path_, pid);

  // Just to be safe when using offsetof, make sure object is standard layout.
  static_assert(std::is_standard_layout<ProcessStats>::value);

  static absl::flat_hash_map<std::string_view, size_t> field_name_to_offset_map{
      {"rchar", offsetof(ProcessStats, rchar_bytes)},
      {"wchar", offsetof(ProcessStats, wchar_bytes)},
      {"read_bytes", offsetof(ProcessStats, read_bytes)},
      {"write_bytes", offsetof(ProcessStats, write_bytes)},
  };

  return ParseFromKeyValueFile(fpath, field_name_to_offset_map, reinterpret_cast<uint8_t*>(out));
}

Status ProcParser::ParseProcStat(SystemStats* out) const {
  /**
   * Sample file:
   * cpu  248758 4995 78314 12965346 10040 0 5498 0 0 0
   * cpu0 43574 817 13011 2159486 994 0 1022 0 0 0
   * ...
   */
  CHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/stat", proc_base_path_);
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
  }

  std::string line;
  bool ok = true;
  while (std::getline(ifs, line)) {
    std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());

    if (!split.empty() && split[0] == "cpu") {
      if (split.size() < kProcStatCPUNumFields) {
        return error::Unknown("Incorrect number of fields in proc/stat CPU");
      }

      ok &= absl::SimpleAtoi(split[KProcStatCPUKTimeField], &out->cpu_ktime_ns);
      ok &= absl::SimpleAtoi(split[KProcStatCPUUTimeField], &out->cpu_utime_ns);

      if (!ok) {
        return error::Unknown("Failed to parse proc/stat cpu info");
      }
      // We only need cpu. We can exit here.
      return Status::OK();
    }
  }

  // If we get here, we failed to extract system information.
  return error::NotFound("Could not extract system information");
}

Status ProcParser::ParseProcMemInfo(SystemStats* out) const {
  /**
   * Sample file:
   *   MemTotal:       65652452 kB
   *   MemFree:        19170960 kB
   *   MemAvailable:   52615288 kB
   * ...
   */
  CHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/meminfo", proc_base_path_);

  // Just to be safe when using offsetof, make sure object is standard layout.
  static_assert(std::is_standard_layout<SystemStats>::value);

  // clang-format off
  static absl::flat_hash_map<std::string_view, size_t> field_name_to_offset_map {
      {"MemTotal", offsetof(SystemStats, mem_total_bytes)},
      {"MemFree", offsetof(SystemStats, mem_free_bytes)},
      {"MemAvailable", offsetof(SystemStats, mem_available_bytes)},
      {"Buffers", offsetof(SystemStats, mem_buffer_bytes)},
      {"Cached", offsetof(SystemStats, mem_cached_bytes)},
      {"SwapCached", offsetof(SystemStats, mem_swap_cached_bytes)},
      {"Active", offsetof(SystemStats, mem_active_bytes)},
      {"Inactive", offsetof(SystemStats, mem_inactive_bytes)},
  };
  // clang-format on

  return ParseFromKeyValueFile(fpath, field_name_to_offset_map, reinterpret_cast<uint8_t*>(out));
}

Status ProcParser::ParseProcPIDStatus(int32_t pid, ProcessStatus* out) const {
  /**
   * Sample file:
   *   Name:	vim
   *   Umask:	0002
   *   State:	S (sleeping)
   * ...
   *   TracerPid:	0
   *   Uid:	1004	1004	1004	1004
   *   Gid:	1004	1004	1004	1004
   * ...
   *   VmPeak:	   24612 kB
   *   VmSize:	   24612 kB
   * ...
   */
  CHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/$1/status", proc_base_path_, pid);

  // Just to be safe when using offsetof, make sure object is standard layout.
  static_assert(std::is_standard_layout<ProcessStatus>::value);

  // clang-format off
  static absl::flat_hash_map<std::string_view, size_t> field_name_to_offset_map {
      {"VmPeak", offsetof(ProcessStatus, vm_peak_bytes)},
      {"VmSize", offsetof(ProcessStatus, vm_size_bytes)},
      {"VmLck", offsetof(ProcessStatus, vm_lck_bytes)},
      {"VmPin", offsetof(ProcessStatus, vm_pin_bytes)},
      {"VmHWM", offsetof(ProcessStatus, vm_hwm_bytes)},
      {"VmRSS", offsetof(ProcessStatus, vm_rss_bytes)},
      {"RssAnon", offsetof(ProcessStatus, rss_anon_bytes)},
      {"RssFile", offsetof(ProcessStatus, rss_file_bytes)},
      {"RssShmem", offsetof(ProcessStatus, rss_shmem_bytes)},
      {"VmData", offsetof(ProcessStatus, vm_data_bytes)},
      {"VmStk", offsetof(ProcessStatus, vm_stk_bytes)},
      {"VmExe", offsetof(ProcessStatus, vm_exe_bytes)},
      {"VmLib", offsetof(ProcessStatus, vm_lib_bytes)},
      {"VmPTE", offsetof(ProcessStatus, vm_pte_bytes)},
      {"VmSwap", offsetof(ProcessStatus, vm_swap_bytes)},
      {"HugetlbPages", offsetof(ProcessStatus, hugetlb_pages_bytes)},
      {"voluntary_ctxt_switches", offsetof(ProcessStatus, voluntary_ctxt_switches)},
      {"nonvoluntary_ctxt_switches", offsetof(ProcessStatus, nonvoluntary_ctxt_switches)},
  };
  // clang-format on

  return ParseFromKeyValueFile(fpath, field_name_to_offset_map, reinterpret_cast<uint8_t*>(out));
}

Status ProcParser::ParseProcPIDSMaps(int32_t pid, std::vector<ProcessSMaps>* out) const {
  CHECK(out != nullptr);
  std::string fpath = absl::Substitute("$0/$1/smaps", proc_base_path_, pid);

  // Just to be safe when using offsetof, make sure object is standard layout.
  static_assert(std::is_standard_layout<ProcessSMaps>::value);

  // clang-format off
  static absl::flat_hash_map<std::string_view, size_t> field_name_to_offset_map {
      {"Size", offsetof(ProcessSMaps, size_bytes)},
      {"KernelPageSize", offsetof(ProcessSMaps, kernel_page_size_bytes)},
      {"MMUPageSize", offsetof(ProcessSMaps, mmu_page_size_bytes)},
      {"Rss", offsetof(ProcessSMaps, rss_bytes)},
      {"Pss", offsetof(ProcessSMaps, pss_bytes)},
      {"Shared_Clean", offsetof(ProcessSMaps, shared_clean_bytes)},
      {"Shared_Dirty", offsetof(ProcessSMaps, shared_dirty_bytes)},
      {"Private_Clean", offsetof(ProcessSMaps, private_clean_bytes)},
      {"Private_Dirty", offsetof(ProcessSMaps, private_dirty_bytes)},
      {"Referenced", offsetof(ProcessSMaps, referenced_bytes)},
      {"Anonymous", offsetof(ProcessSMaps, anonymous_bytes)},
      {"LazyFree", offsetof(ProcessSMaps, lazy_free_bytes)},
      {"AnonHugePages", offsetof(ProcessSMaps, anon_huge_pages_bytes)},
      {"ShmemPmdMapped", offsetof(ProcessSMaps, shmem_pmd_mapped_bytes)},
      {"FilePmdMapped", offsetof(ProcessSMaps, file_pmd_mapped_bytes)},
      {"Shared_Hugetlb", offsetof(ProcessSMaps, shared_hugetlb_bytes)},
      {"Private_Hugetlb", offsetof(ProcessSMaps, private_hugetlb_bytes)},
      {"Swap", offsetof(ProcessSMaps, swap_bytes)},
      {"SwapPss", offsetof(ProcessSMaps, swap_pss_bytes)},
      {"Locked", offsetof(ProcessSMaps, locked_bytes)},
  };
  // clang-format on

  static constexpr int kProcMapNumFields = 6;

  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
  }

  std::string line;
  while (std::getline(ifs, line)) {
    // We need to match the header lines which are of the following form:
    // address                   perms offset   dev    inode             pathname
    // For example:
    // 55e816b37000-55e816b65000 r--p  00000000 103:02 55579316          /usr/bin/vim.basic
    // We differentiate these headers from the subsequent key-value pairs that include
    // data about the memory usage for each of the process mappings.
    auto idx = line.find(':');
    if (idx == std::string::npos) {
      continue;
    }
    // If the character after the colon is whitespace, this is a key-value line.
    // Else the colon is part of the device (major:minor) and this is a header.
    // Perhaps we should look for other indicators?
    if (idx + 1 < line.length() && !absl::ascii_isspace(line[idx + 1])) {
      std::vector<std::string_view> split =
          absl::StrSplit(line, absl::MaxSplits(' ', kProcMapNumFields), absl::SkipWhitespace());
      // We might end up with 5 or 6 fields based on whether we have a pathname or not.
      if (split.size() < kProcMapNumFields - 1) {
        return error::Internal("Failed to parse file $0", fpath);
      }
      auto& smap_info = out->emplace_back();
      smap_info.address = split[0];
      smap_info.offset = split[2];
      smap_info.pathname = "[anonymous]";
      if (split.size() == kProcMapNumFields) {
        smap_info.pathname = absl::StripAsciiWhitespace(split[kProcMapNumFields - 1]);
      }
      continue;
    }
    ParseFromKeyValueLine(line, field_name_to_offset_map, reinterpret_cast<uint8_t*>(&out->back()));
  }

  return Status::OK();
}

void ProcParser::ParseFromKeyValueLine(
    const std::string& line,
    const absl::flat_hash_map<std::string_view, size_t>& field_name_to_value_map,
    uint8_t* out_base) {
  std::vector<std::string_view> split = absl::StrSplit(line, ':', absl::SkipWhitespace());
  if (split.size() >= 2) {
    const auto& key = split[0];
    const auto& val = split[1];

    const auto& it = field_name_to_value_map.find(key);
    // Key not found in map, we can just go to next iteration of loop.
    if (it == field_name_to_value_map.end()) {
      return;
    }

    size_t offset = it->second;
    auto val_ptr = reinterpret_cast<int64_t*>(out_base + offset);

    bool ok = false;
    if (absl::EndsWith(val, " kB")) {
      // Convert kB to bytes. proc seems to only use kB as the unit if it's present
      // else there are no units.
      const std::string_view trimmed_val = absl::StripSuffix(val, " kB");
      ok = absl::SimpleAtoi(trimmed_val, val_ptr);
      *val_ptr *= 1024;
    } else {
      ok = absl::SimpleAtoi(val, val_ptr);
    }

    if (!ok) {
      *val_ptr = -1;
    }
  }
  return;
}

Status ProcParser::ParseFromKeyValueFile(
    const std::string& fpath,
    const absl::flat_hash_map<std::string_view, size_t>& field_name_to_value_map,
    uint8_t* out_base) {
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath);
  }

  std::string line;
  size_t read_count = 0;
  while (std::getline(ifs, line)) {
    ParseFromKeyValueLine(line, field_name_to_value_map, out_base);

    // Check to see if we have read all the fields, if so we can skip the
    // rest. We assume no duplicates.
    if (read_count == field_name_to_value_map.size()) {
      break;
    }
  }

  return Status::OK();
}

std::string ProcParser::GetPIDCmdline(int32_t pid) const {
  std::string fpath = absl::Substitute("$0/$1/cmdline", proc_base_path_, pid);
  std::ifstream ifs(fpath);
  if (!ifs) {
    return "";
  }

  std::string line = "";
  std::string cmdline = "";
  while (std::getline(ifs, line)) {
    cmdline += std::move(line);
  }

  // Strip out extra null character at the end of the string.
  if (!cmdline.empty() && cmdline[cmdline.size() - 1] == 0) {
    cmdline.pop_back();
  }

  // Replace all nulls with spaces. Sometimes the command line has
  // null to separate arguments and others it has spaces. We just make them all spaces
  // and leave it to upstream code to tokenize properly.
  std::replace(cmdline.begin(), cmdline.end(), static_cast<char>(0), ' ');

  return cmdline;
}

StatusOr<std::filesystem::path> ProcParser::GetExePath(int32_t pid) const {
  auto exe_path = std::filesystem::path(proc_base_path_) / std::to_string(pid) / "exe";
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, fs::ReadSymlink(exe_path));
  if (proc_exe.empty() || proc_exe == "/") {
    // Not sure what causes this, but sometimes get symlinks that point to "/".
    // Seems to happen with PIDs that are short-lived, because I can never catch it in the act.
    // Suspect there is a race with the proc filesystem, with PID creation/destruction,
    // but this is not confirmed. Would be nice to understand the root cause, but for now, just
    // filter these out.
    return error::Internal("Symlink appears malformed.");
  }
  return proc_exe;
}

StatusOr<int64_t> ProcParser::GetPIDStartTimeTicks(int32_t pid) const {
  const std::filesystem::path proc_pid_path =
      std::filesystem::path(proc_base_path_) / std::to_string(pid);
  return ::px::system::GetPIDStartTimeTicks(proc_pid_path);
}

Status ProcParser::ReadProcPIDFDLink(int32_t pid, int32_t fd, std::string* out) const {
  std::string fpath = absl::Substitute("$0/$1/fd/$2", proc_base_path_, pid, fd);
  PL_ASSIGN_OR_RETURN(std::filesystem::path link, fs::ReadSymlink(fpath));
  *out = std::move(link);
  return Status::OK();
}

std::string_view LineWithPrefix(std::string_view content, std::string_view prefix) {
  const std::vector<std::string_view> lines = absl::StrSplit(content, "\n");
  for (const auto& line : lines) {
    if (absl::StartsWith(line, prefix)) {
      return line;
    }
  }
  return {};
}

// Looking for UIDs in <proc_path>/<pid>/status, the content looks like:
// $ cat /proc/2578/status
// Name:  apache2
// Umask: 0022
// State: S (sleeping)
// ...
// Uid: 33 33 33 33
// ...
Status ProcParser::ReadUIDs(int32_t pid, ProcUIDs* uids) const {
  std::filesystem::path proc_pid_status_path =
      std::filesystem::path(proc_base_path_) / std::to_string(pid) / "status";
  PL_ASSIGN_OR_RETURN(std::string content, px::ReadFileToString(proc_pid_status_path));

  constexpr std::string_view kUIDPrefix = "Uid:";
  std::string_view uid_line = LineWithPrefix(content, kUIDPrefix);
  std::vector<std::string_view> fields =
      absl::StrSplit(uid_line, absl::ByAnyChar(kFieldSeparators), absl::SkipEmpty());
  constexpr size_t kFieldCount = 5;
  if (fields.size() != kFieldCount) {
    return error::Internal("Proc path '$0' returns incorrect result '$1'",
                           proc_pid_status_path.string(), uid_line);
  }
  uids->real = fields[1];
  uids->effective = fields[2];
  uids->saved_set = fields[3];
  uids->filesystem = fields[4];
  return Status::OK();
}

// Looking for NSpid: in <proc_path>/<pid>/status, the content looks like:
// $ cat /proc/2578/status
// Name:  apache2
// Umask: 0022
// State: S (sleeping)
// ...
// NSpid: 33 33
// ...
//
// There may not be a second pid if the process is not running inside a namespace.
Status ProcParser::ReadNSPid(pid_t pid, std::vector<std::string>* ns_pids) const {
  std::filesystem::path proc_pid_status_path =
      std::filesystem::path(proc_base_path_) / std::to_string(pid) / "status";
  PL_ASSIGN_OR_RETURN(std::string content, px::ReadFileToString(proc_pid_status_path));

  constexpr std::string_view kNSPidPrefix = "NStgid:";
  std::string_view ns_pid_line = LineWithPrefix(content, kNSPidPrefix);
  std::vector<std::string_view> fields =
      absl::StrSplit(ns_pid_line, absl::ByAnyChar(kFieldSeparators), absl::SkipEmpty());
  if (fields.size() < 2) {
    return error::InvalidArgument("NSpid line in '$0' is invalid: '$1'",
                                  proc_pid_status_path.string(), ns_pid_line);
  }
  for (size_t i = 1; i < fields.size(); ++i) {
    ns_pids->push_back(std::string(fields[i]));
  }
  return Status::OK();
}

StatusOr<int64_t> GetPIDStartTimeTicks(const std::filesystem::path& proc_pid_path) {
  const std::filesystem::path proc_pid_stat_path = proc_pid_path / "stat";
  const std::string fpath = proc_pid_stat_path.string();

  std::string line;

  // It's usually a big no-no to ifdef based on compilation mode,
  // but this is a workaround for an ASAN bug that causes crashes and flakiness
  // in our tests. See //src/common/system/proc_parser_bug_test.cc for details.
  //
  // The workaround implementation uses a C functions that don't throw exceptions.
  // Why not just use the workaround version all the time?
  // Because it has a hard-coded constant which makes it less robust.
#if defined(PL_CONFIG_ASAN)
  std::FILE* fp = std::fopen(fpath.c_str(), "r");
  if (fp == nullptr) {
    return error::Internal("Could not open file $0", fpath);
  }

  // A few quick runs show that /proc/<pid>/stat is <200 characters on my machine,
  // but a lot of those numbers are 0. If they were non-zero, the string could be much
  // longer. Hopefully a 4KB block keeps us safe. This is for ASAN only anyways.
  constexpr int kMaxSupportedProcPIDStatLength = 4096;
  line.resize(kMaxSupportedProcPIDStatLength);
  if (std::fgets(line.data(), line.size(), fp) == nullptr) {
    return error::Internal("Could not get line from file $0", fpath);
  }
#else
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Could not open file $0", fpath);
  }

  if (!std::getline(ifs, line)) {
    return error::Internal("Could not get line from file $0", fpath);
  }
#endif

  std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
  // We check less than in case more fields are added later.
  if (split.size() < kProcStatNumFields) {
    return error::Internal("Unexpected number of columns in file $0 [columns = $1].", fpath,
                           split.size());
  }

  int64_t start_time_ticks;
  if (!absl::SimpleAtoi(split[kProcStatStartTimeField], &start_time_ticks)) {
    return error::Internal("Time value does not parse in file $0", fpath);
  }

  return start_time_ticks;
}

namespace {

Status ParseMountInfo(std::string_view str, ProcParser::MountInfo* mount_info) {
  std::vector<std::string_view> fields =
      absl::StrSplit(str, absl::ByAnyChar(kFieldSeparators), absl::SkipWhitespace());
  if (fields.size() < 10) {
    return error::InvalidArgument("Mountinfo record should have at least 10 fields, got: $0",
                                  fields.size());
  }
  mount_info->dev = fields[2];
  mount_info->root = fields[3];
  mount_info->mount_point = fields[4];

  return Status::OK();
}

}  // namespace

Status ProcParser::ReadMountInfos(pid_t pid,
                                  std::vector<ProcParser::MountInfo>* mount_infos) const {
  const std::filesystem::path proc_pid_mount_info_path = ProcPidPath(pid) / "mountinfo";
  PL_ASSIGN_OR_RETURN(std::string content, px::ReadFileToString(proc_pid_mount_info_path));
  std::vector<std::string_view> lines = absl::StrSplit(content, "\n", absl::SkipWhitespace());
  for (const auto line : lines) {
    ProcParser::MountInfo& mount_info = mount_infos->emplace_back();
    PL_RETURN_IF_ERROR(ParseMountInfo(line, &mount_info));
  }
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> ProcParser::GetMapPaths(pid_t pid) const {
  static constexpr int kProcMapNumFields = 6;
  absl::flat_hash_set<std::string> map_paths;

  const std::filesystem::path proc_pid_maps_path = ProcPidPath(pid) / "maps";
  PL_ASSIGN_OR_RETURN(std::string content, px::ReadFileToString(proc_pid_maps_path));
  std::vector<std::string_view> lines = absl::StrSplit(content, "\n", absl::SkipWhitespace());
  for (const auto line : lines) {
    std::vector<std::string_view> fields =
        absl::StrSplit(line, absl::MaxSplits(' ', kProcMapNumFields), absl::SkipWhitespace());

    if (fields.size() == kProcMapNumFields) {
      std::string pathname(fields[kProcMapNumFields - 1]);
      absl::StripAsciiWhitespace(&pathname);
      map_paths.insert(std::move(pathname));
    }
  }
  return map_paths;
}

}  // namespace system
}  // namespace px
