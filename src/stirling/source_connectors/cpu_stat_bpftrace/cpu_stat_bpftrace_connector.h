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

#include "src/stirling/core/source_connector.h"

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

namespace px {
namespace stirling {

class CPUStatBPFTraceConnector : public SourceConnector, public bpf_tools::BPFTraceWrapper {
 public:
  // clang-format off
  static constexpr DataElement kElements[] = {
      {
        "time_",
        "Timestamp when the data record was collected.",
        types::DataType::TIME64NS,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_user",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_nice",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_system",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_idle",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_iowait",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_irq",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_softirq",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      }
  };
  // clang-format on
  static constexpr auto kTable = DataTableSchema(
      "bpftrace_cpu_stats", "CPU usage metrics for processes (obtained via BPFtrace)", kElements);
  static constexpr auto kTables = MakeArray(kTable);
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    // TODO(oazizi): Expose cpu_id through Create.
    return std::unique_ptr<SourceConnector>(new CPUStatBPFTraceConnector(name, /* cpu_id */ 0));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;

 protected:
  CPUStatBPFTraceConnector(std::string_view name, uint64_t cpu_id)
      : SourceConnector(name, kTables), cpu_id_(cpu_id) {}

 private:
  uint64_t cpu_id_ = 0;
};

}  // namespace stirling
}  // namespace px
