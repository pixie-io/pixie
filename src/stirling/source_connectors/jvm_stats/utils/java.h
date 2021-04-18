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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "src/common/base/statusor.h"

namespace px {
namespace stirling {
namespace java {

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
 * Returns the path of the hsperfdata for a JVM process.
 */
StatusOr<std::filesystem::path> HsperfdataPath(pid_t pid);

}  // namespace java
}  // namespace stirling
}  // namespace px
