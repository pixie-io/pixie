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

#include <prometheus/counter.h>

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/proc_exit/bcc_bpf_intf/proc_exit.h"
#include "src/stirling/source_connectors/proc_exit/proc_exit_events_table.h"
#include "src/stirling/utils/monitor.h"

namespace px {
namespace stirling {
namespace proc_exit_tracer {

// This connector is not registered yet, so it has no effect.
class ProcExitConnector : public BCCSourceConnector {
 public:
  static constexpr std::string_view kName = "proc_exit_tracer";

  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  static constexpr auto kTables = MakeArray(kProcExitEventsTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new ProcExitConnector(name));
  }

  ProcExitConnector() = delete;
  ~ProcExitConnector() override = default;

  void AcceptProcExitEvent(const struct proc_exit_event_t& event);

 protected:
  explicit ProcExitConnector(std::string_view name);

  Status InitImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;
  Status StopImpl() override { return Status::OK(); }

 private:
  std::vector<struct proc_exit_event_t> events_;

 private:
  // Update counters related to java process.
  void UpdateCrashedJavaProcCounters(
      uint32_t asid, const proc_exit_event_t& event,
      const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& upid_pid_info_map);

  prometheus::Counter& java_proc_crashed_counter_;
  prometheus::Counter& java_proc_crashed_with_profiler_counter_;
  prometheus::Counter& java_proc_crashed_without_profiler_counter_;
  StirlingMonitor& monitor_ = *StirlingMonitor::GetInstance();
};

}  // namespace proc_exit_tracer
}  // namespace stirling
}  // namespace px
