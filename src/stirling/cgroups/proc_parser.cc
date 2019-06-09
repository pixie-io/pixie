#include <experimental/filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/stirling/cgroups/proc_parser.h"

namespace pl {
namespace stirling {

/**
 * These constants are used to ignore virtual and local network interfaces.
 */
const std::vector<std::string> kNetIFaceIgnorePrefix = {
    "v",
    "docker",
    "lo",
};

/*************************************************
 * Constants for the /proc/stat file
 *************************************************/
const int kProcStatCPUNumFields = 11;
const int KProcStatCPUUTimeField = 1;
const int KProcStatCPUKTimeField = 3;

/*************************************************
 * Constants for the /proc/<pid>/net/dev file
 *************************************************/
const int kProcNetDevNumFields = 17;

const int kProcNetDevIFaceField = 0;
const int kProcNetDevRxBytesField = 1;
const int kProcNetDevRxPacketsField = 2;
const int kProcNetDevRxErrsField = 3;
const int kProcNetDevRxDropField = 4;

const int kProcNetDevTxBytesField = 10;
const int kProcNetDevTxPacketsField = 11;
const int kProcNetDevTxErrsField = 12;
const int kProcNetDevTxDropField = 13;

/*************************************************
 * Constants for the /proc/<pid>/stat file
 *************************************************/
const int kProcStatNumFields = 52;

const int kProcStatPIDField = 0;
const int kProcStatProcessNameField = 1;

const int kProcStatMinorFaultsField = 9;
const int kProcStatMajorFaultsField = 11;

const int kProcStatUTimeField = 13;
const int kProcStatKTimeField = 14;
const int kProcStatNumThreadsField = 19;

const int kProcStatVSizeField = 22;
const int kProcStatRSSField = 23;

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

bool ShouldSkipNetIFace(const std::string_view iface) {
  // TODO(zasgar): We might want to make this configurable at some point.
  for (const auto& prefix : kNetIFaceIgnorePrefix) {
    if (absl::StartsWith(iface, prefix)) {
      return true;
    }
  }
  return false;
}

fs::path ProcParser::GetProcStatFilePath() { return proc_base_path_ / fs::path("stat"); }

fs::path ProcParser::GetProcPidStatFilePath(int64_t pid) {
  return proc_base_path_ / std::to_string(pid) / fs::path("stat");
}

fs::path ProcParser::GetProcPidStatIOFile(int64_t pid) {
  return proc_base_path_ / std::to_string(pid) / fs::path("io");
}

fs::path ProcParser::GetProcPidNetDevFile(int64_t pid) {
  return proc_base_path_ / std::to_string(pid) / fs::path("net") / fs::path("dev");
}

Status ProcParser::ParseProcPIDNetDev(const fs::path& fpath, NetworkStats* out) {
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

  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath.string());
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

    if (ShouldSkipNetIFace(split[kProcNetDevIFaceField])) {
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

Status ProcParser::ParseProcPIDStat(const fs::path& fpath, ProcessStats* out) {
  /**
   * Sample file:
   * 4602 (ibazel) S 3260 4602 3260 34818 4602 1077936128 1799 174589 \
   * 55 68 8 23 106 72 20 0 13 0 14329 114384896 2577 18446744073709551615 \
   * 4194304 7917379 140730842479232 0 0 0 1006254592 0 2143420159 0 0 0 17 \
   * 3 0 0 3 0 0 12193792 12432192 34951168 140730842488151 140730842488200 \
   * 140730842488200 140730842492896 0
   */
  DCHECK(out != nullptr);

  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath.string());
  }

  std::string line;
  bool ok = true;
  if (std::getline(ifs, line)) {
    std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
    // We check less than in case more fields are added later.
    if (split.size() < kProcStatNumFields) {
      return error::Unknown("Incorrect number of fields in stat file: $0", fpath.string());
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
    // The kernel tracks utime and ktime in jiffies.
    out->utime_ns *= ns_per_kernel_tick_;
    out->ktime_ns *= ns_per_kernel_tick_;

    ok &= absl::SimpleAtoi(split[kProcStatNumThreadsField], &out->num_threads);
    ok &= absl::SimpleAtoi(split[kProcStatVSizeField], &out->vsize_bytes);
    ok &= absl::SimpleAtoi(std::string(split[kProcStatRSSField]), &out->rss_bytes);

    // RSS is in pages.
    out->rss_bytes *= bytes_per_page_;

  } else {
    return error::Internal("Failed to read proc stat file: $0", fpath.string());
  }

  if (!ok) {
    // This should never happen since it requires the file to be ill-formed
    // by the kernel.
    return error::Internal("failed to parse stat file ($0). ATOI failed.", fpath.string());
  }
  return Status::OK();
}

// WARNING: since this function uses a static variable, it is not thread-safe.
Status ProcParser::ParseProcPIDStatIO(const fs::path& fpath, ProcessStats* out) {
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

  // Just to be safe when using offsetof, make sure object is standard layout.
  static_assert(std::is_standard_layout<ProcessStats>::value);

  static std::unordered_map<std::string_view, size_t> field_name_to_offset_map = {
      {"rchar:", offsetof(ProcessStats, rchar_bytes)},
      {"wchar:", offsetof(ProcessStats, wchar_bytes)},
      {"read_bytes:", offsetof(ProcessStats, read_bytes)},
      {"write_bytes:", offsetof(ProcessStats, write_bytes)},
  };

  return ParseFromKeyValueFile(fpath, field_name_to_offset_map, reinterpret_cast<uint8_t*>(out),
                               1 /*field_value_multipler*/);
}

Status ProcParser::ParseProcStat(const fs::path& fpath, SystemStats* out) {
  /**
   * Sample file:
   * cpu  248758 4995 78314 12965346 10040 0 5498 0 0 0
   * cpu0 43574 817 13011 2159486 994 0 1022 0 0 0
   * ...
   */
  CHECK(out != nullptr);

  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath.string());
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

// WARNING: since this function uses a static variable, it is not thread-safe.
Status ProcParser::ParseProcMemInfo(const fs::path& fpath, SystemStats* out) {
  /**
   * Sample file:
   *   MemTotal:       65652452 kB
   *   MemFree:        19170960 kB
   *   MemAvailable:   52615288 kB
   * ...
   */
  CHECK(out != nullptr);

  // Just to be safe when using offsetof, make sure object is standard layout.
  static_assert(std::is_standard_layout<SystemStats>::value);

  // clang-format off
  static std::unordered_map<std::string_view, size_t> field_name_to_offset_map = {
      {"MemTotal:", offsetof(SystemStats, mem_total_bytes)},
      {"MemFree:", offsetof(SystemStats, mem_free_bytes)},
      {"MemAvailable:", offsetof(SystemStats, mem_available_bytes)},
      {"Buffers:", offsetof(SystemStats, mem_buffer_bytes)},
      {"Cached:", offsetof(SystemStats, mem_cached_bytes)},
      {"SwapCached:", offsetof(SystemStats, mem_swap_cached_bytes)},
      {"Active:", offsetof(SystemStats, mem_active_bytes)},
      {"Inactive:", offsetof(SystemStats, mem_inactive_bytes)},
  };
  // clang-format on

  // This is a key value pair with a unit (that is always KB when present).
  constexpr int kKBToByteMultiplier = 1024;
  return ParseFromKeyValueFile(fpath, field_name_to_offset_map, reinterpret_cast<uint8_t*>(out),
                               kKBToByteMultiplier);
}

Status ProcParser::ParseFromKeyValueFile(
    const fs::path& fpath,
    const std::unordered_map<std::string_view, size_t>& field_name_to_value_map, uint8_t* out_base,
    int64_t field_value_multiplier) {
  std::ifstream ifs;
  ifs.open(fpath);
  if (!ifs) {
    return error::Internal("Failed to open file $0", fpath.string());
  }

  std::string line;
  size_t read_count = 0;
  while (std::getline(ifs, line)) {
    std::vector<std::string_view> split = absl::StrSplit(line, " ", absl::SkipWhitespace());
    // This is a key value pair with a unit (that is always KB when present).
    // If the number is 0 then the units are missing so we either have 2 or 3
    // for the width of the field.
    const int kMemInfoMinFields = 2;
    const int kMemInfoMaxFields = 3;

    if (split.size() >= kMemInfoMinFields && split.size() <= kMemInfoMaxFields) {
      const auto& key = split[0];
      const auto& val = split[1];

      const auto& it = field_name_to_value_map.find(key);
      // Key not found in map, we can just go to next iteration of loop.
      if (it == field_name_to_value_map.end()) {
        continue;
      }

      size_t offset = it->second;
      int64_t* val_ptr = reinterpret_cast<int64_t*>(out_base + offset);
      bool ok = absl::SimpleAtoi(val, val_ptr);
      *val_ptr *= field_value_multiplier;

      if (!ok) {
        return error::Unknown("Failed to parse proc/meminfo");
      }

      // Check to see if we have read all the fields, if so we can skip the
      // rest. We assume no duplicates.
      if (read_count == field_name_to_value_map.size()) {
        break;
      }
    }
  }

  return Status::OK();
}

}  // namespace stirling
}  // namespace pl
