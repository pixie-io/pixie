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

#include <memory>
#include <string>
#include <unordered_map>

#include <magic_enum.hpp>

#include "src/stirling/source_connectors/socket_tracer/metrics.h"

namespace px {
namespace stirling {

SocketTracerMetrics::SocketTracerMetrics(prometheus::Registry* registry,
                                         traffic_protocol_t protocol,
                                         bool tls)
    : data_loss_bytes(
          prometheus::BuildCounter()
              .Name("data_loss_bytes")
              .Help("Total bytes of data loss for this protocol. Measured by bytes that weren't "
                    "successfully parsed.")
              .Register(*registry)
              .Add({
                  {"protocol", std::string(magic_enum::enum_name(protocol))},
                  {"tls", tls ? "1" : "0"},
              })),
      conn_stats_bytes(prometheus::BuildCounter()
                           .Name("conn_stats_bytes")
                           .Help("Total bytes of data tracked by conn stats for this protocol.")
                           .Register(*registry)
                           .Add({
                               {"protocol", std::string(magic_enum::enum_name(protocol))},
                               {"tls", tls ? "1" : "0"},
                           })) {}

namespace {
std::unordered_map<traffic_protocol_t, std::unique_ptr<SocketTracerMetrics>> g_plaintext_protocol_metrics;

std::unordered_map<traffic_protocol_t, std::unique_ptr<SocketTracerMetrics>> g_encrypted_protocol_metrics;

std::unordered_map<traffic_protocol_t, std::unique_ptr<SocketTracerMetrics>>* GetUnderlyingProtocolMetrics(bool tls) {
  if (tls) {
    return &g_encrypted_protocol_metrics;
  } else {
    return &g_plaintext_protocol_metrics;
  }
}

void ResetProtocolMetrics(traffic_protocol_t protocol, bool tls) {
  GetUnderlyingProtocolMetrics(tls)->insert_or_assign(
      protocol, std::make_unique<SocketTracerMetrics>(&GetMetricsRegistry(), protocol, tls));
}
}  // namespace

SocketTracerMetrics& SocketTracerMetrics::GetProtocolMetrics(traffic_protocol_t protocol, bool tls) {
  auto metrics = GetUnderlyingProtocolMetrics(tls);
  if (metrics->find(protocol) == metrics->end()) {
    ResetProtocolMetrics(protocol, tls);
  }
  return *(*metrics)[protocol];
}

void SocketTracerMetrics::TestOnlyResetProtocolMetrics(traffic_protocol_t protocol, bool tls) {
  ResetProtocolMetrics(protocol, tls);
}

}  // namespace stirling
}  // namespace px
