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

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bpftrace_wrapper.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

class PIDCPUUseBPFTraceConnector : public SourceConnector, public bpf_tools::BPFTraceWrapper {
 public:
  static constexpr std::string_view kName = "pid_runtime_bpftrace";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  // clang-format off
  static constexpr DataElement kElements[] = {
      {
        "time_",
        "",
        types::DataType::TIME64NS,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "pid",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
      },
      {
        "runtime_ns",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cmd",
        "",
        types::DataType::STRING,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
      }
  };
  // clang-format on
  static constexpr auto kTable =
      DataTableSchema("bpftrace_pid_cpu_usage",
                      "CPU usage metrics for processes (obtained via BPFtrace)", kElements);
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PIDCPUUseBPFTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;

 private:
  explicit PIDCPUUseBPFTraceConnector(std::string_view name) : SourceConnector(name, kTables) {}

  bpftrace::BPFTraceMap last_result_times_;

  bpftrace::BPFTraceMap::iterator BPFTraceMapSearch(const bpftrace::BPFTraceMap& vector,
                                                    bpftrace::BPFTraceMap::iterator it,
                                                    uint64_t search_key);
};

}  // namespace stirling
}  // namespace px
