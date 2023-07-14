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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/pid_runtime/bcc_bpf_intf/pidruntime.h"

namespace px {
namespace stirling {

class PIDRuntimeConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  using BPFMapDataT = bpf_tools::WrappedBCCMap<uint16_t, pidruntime_val_t>;

  static constexpr std::string_view kName = "bcc_cpu_stat";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  // clang-format off
  static constexpr DataElement kElements[] = {
      canonical_data_elements::kTime,
      // TODO(yzhao): Change to upid.
      {"pid", "Process PID",
       types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      // TODO(chengruizhe): Convert to counter.
      {"runtime_ns", "Process runtime in nanoseconds",
       types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
      {"cmd", "Process command line",
       types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
  };
  // clang-format on

  static constexpr auto kTable = DataTableSchema(
      "bcc_pid_cpu_usage", "CPU usage metrics for processes (obtained via BPF)", kElements);

  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PIDRuntimeConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;

 protected:
  explicit PIDRuntimeConnector(std::string_view name)
      : SourceConnector(name, kTables), bpf_tools::BCCWrapper() {}

 private:
  // Freq. (in Hz) at which to trigger bpf func.
  static constexpr uint64_t kSamplingFreqHz = 99;
  static constexpr auto kSamplingProbes =
      MakeArray<bpf_tools::SamplingProbeSpec>({"trace_pid_runtime", kSamplingFreqHz});

  std::map<uint16_t, uint64_t> prev_run_time_map_;
  std::unique_ptr<BPFMapDataT> bpf_data_;
};

}  // namespace stirling
}  // namespace px
