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

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

class ProcStatConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "proc_stat";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  // clang-format off
  static constexpr DataElement kElements[] = {
      canonical_data_elements::kTime,
      {"system_percent", "The percentage of time the CPU was executing in kernel-space",
       types::DataType::FLOAT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
      {"user_percent", "The percentage of time the CPU was executing in user-space",
       types::DataType::FLOAT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
      {"idle_percent", "The percentage of time the CPU was idle",
       types::DataType::FLOAT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE}
  };
  // clang-format on
  static constexpr auto kTable = DataTableSchema(
      "proc_stat", "CPU usage metrics for processes (obtained via /proc)", kElements);

  static constexpr auto kTables = MakeArray(kTable);

  ProcStatConnector() = delete;
  ~ProcStatConnector() override = default;
  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new ProcStatConnector(name));
  }

 protected:
  explicit ProcStatConnector(std::string_view name) : SourceConnector(name, kTables) {}
  Status InitImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;

  Status StopImpl() override { return Status::OK(); }

  /**
   * @brief Read /proc/stat and parse the cpu usage stats into a vector of strings
   *
   * @return std::vector<std::string>
   */
  virtual std::vector<std::string> GetProcParams();

  /**
   * @brief Populate a struct of data values. The values generated should adhere to
   * the elements_ types.
   *
   * @param parsed cpu stat vector of strings from /proc/stat
   * @return Status
   */
  Status GetProcStat(const std::vector<std::string>& parsed_str);

  /**
   * @brief Prevent the compiler from padding the cpu_usage_record struct. For every
   * GetDataImpl call, we populate a struct of CpuUsage, cast it as a uint8_t*
   * and create a RawDataBuf.
   *
   */
  struct __attribute__((__packed__)) CPUUsage {
    uint64_t timestamp;
    double system_percent, user_percent, idle_percent;
  };

  struct CPUStat {
    uint64_t total, system, user, idle;
  };

  CPUUsage cpu_usage_ = {0, 0.0, 0.0, 0.0};
  CPUStat prev_cpu_usage_ = {0, 0, 0, 0};
  const int kUserIdx = 1;
  const int kIdleIdx = 4;
  const int kIOWaitIdx = 5;
  const int kNumCPUStatFields = 10;
};

}  // namespace stirling
}  // namespace px
