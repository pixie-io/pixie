#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "src/common/base/statusor.h"

namespace pl {
namespace stirling {

/**
 * Stats holds a map of name and value, and computes meaningful stats to be exported to Stirling.
 * Uses suffixes to match stat name, so that it's robust to different JVM vendors.
 * For example, Azul Zing JVM usual has "azul" as the first component of the name.
 */
class Stats {
 public:
  struct Stat {
    std::string_view name;
    uint64_t value;
  };

  explicit Stats(std::vector<Stat> stats);
  explicit Stats(std::string hsperf_data);

  /**
   * Parses the held hsperf data into structured stats.
   */
  Status Parse();

  uint64_t YoungGCTimeNanos() const;
  uint64_t FullGCTimeNanos() const;
  uint64_t UsedHeapSizeBytes() const;
  uint64_t TotalHeapSizeBytes() const;
  uint64_t MaxHeapSizeBytes() const;

 private:
  uint64_t StatForSuffix(std::string_view suffix) const;
  uint64_t SumStatsForSuffixes(const std::vector<std::string_view>& suffixes) const;

  std::string hsperf_data_;
  std::vector<Stat> stats_;
};

/**
 * Resolve the given path in the context of the process, which may be in a namespace.
 * If the process is in a namespace, the path will be resolved to the host path.
 */
StatusOr<std::filesystem::path> ResolvePidPath(pid_t pid, const std::filesystem::path& path);

/**
 * Returns the path of the hsperfdata for a JVM process.
 */
StatusOr<std::filesystem::path> HsperfdataPath(pid_t pid);

}  // namespace stirling
}  // namespace pl
