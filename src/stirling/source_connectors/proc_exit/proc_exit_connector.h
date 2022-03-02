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

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/proc_exit/bcc_bpf_intf/proc_exit.h"

namespace px {
namespace stirling {

// This connector is not registered yet, so it has no effect.
class ProcExitConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  static constexpr std::string_view kName = "proc_exit";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  // clang-format off
  static constexpr DataElement kElements[] = {
      canonical_data_elements::kTime,
      canonical_data_elements::kUPID,
      {"exit_code",
      "The exit code of this process.",
      types::DataType::INT64,
      types::SemanticType::ST_NONE,
      types::PatternType::GENERAL},
      {"signal",
      "The signal received by this process.",
      types::DataType::INT64,
      types::SemanticType::ST_NONE,
      types::PatternType::GENERAL},
      // TODO(yzhao): This is process name, not command line arguments.
      // Although reading it from /proc/<pid>/exe appears straightforward, but this process has
      // already exited when reading from userspace. One option is to trace
      // sched:sched_process_{exec,fork}, and resolve command line in userspace after detecting new
      // processes.
      {"comm",
      "The command line of this process.",
      types::DataType::STRING,
      types::SemanticType::ST_NONE,
      types::PatternType::GENERAL},
  };
  // clang-format on
  static constexpr auto kTable =
      DataTableSchema("proc_exit", "Traces all abnormal process exits", kElements);
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new ProcExitConnector(name));
  }

  ProcExitConnector() = delete;
  ~ProcExitConnector() override = default;

  void AcceptProcExitEvent(const struct proc_exit_event_t& event);

 protected:
  explicit ProcExitConnector(std::string_view name) : SourceConnector(name, kTables) {}

  Status InitImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, const std::vector<DataTable*>& data_tables) override;
  Status StopImpl() override { return Status::OK(); }

 private:
  std::vector<struct proc_exit_event_t> events_;
};

}  // namespace stirling
}  // namespace px
