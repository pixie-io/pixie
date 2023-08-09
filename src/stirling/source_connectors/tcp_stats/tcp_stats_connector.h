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
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/tcp_stats/tcp_stats.h"
#include "src/stirling/source_connectors/tcp_stats/tcp_stats_table.h"

namespace px {
namespace stirling {

class TCPStatsConnector : public BCCSourceConnector {
 public:
  static constexpr std::string_view kName = "tcp_stats";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kTables = MakeArray(tcp_stats::kTCPStatsTable);
  static constexpr uint32_t kTCPStatsTableNum = TableNum(kTables, tcp_stats::kTCPStatsTable);

  TCPStatsConnector() = delete;
  ~TCPStatsConnector() override = default;

  void AcceptTcpEvent(const struct tcp_event_t& event);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new TCPStatsConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx) override;

 protected:
  explicit TCPStatsConnector(std::string_view name) : BCCSourceConnector(name, kTables) {}

 private:
  std::vector<struct tcp_event_t> events_;
  TCPStats tcp_stats_;
};
}  // namespace stirling
}  // namespace px
