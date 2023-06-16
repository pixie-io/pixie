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
#include <utility>

#include <magic_enum.hpp>

#include "src/stirling/source_connectors/socket_tracer/metrics.h"

using metrics_key = std::pair<traffic_protocol_t, ssl_source_t>;

namespace std {

// Provides hash specialization since stdlib doesn't provide one for std::pair.
// This uses the first 32 bits for the traffic_protocol_t enum and the final 32
// bits for the ssl_source_t.
template <>
struct hash<metrics_key> {
  std::size_t operator()(const metrics_key& p) const {
    uint64_t protocol = std::get<0>(p);
    int32_t tls_source = std::get<1>(p);

    return (protocol << 32) | tls_source;
  }
};

}  // namespace std

namespace px {
namespace stirling {

SocketTracerMetrics::SocketTracerMetrics(prometheus::Registry* registry,
                                         traffic_protocol_t protocol, ssl_source_t tls_source)
    : data_loss_bytes(
          prometheus::BuildCounter()
              .Name("data_loss_bytes")
              .Help("Total bytes of data loss for this protocol. Measured by bytes that weren't "
                    "successfully parsed.")
              .Register(*registry)
              .Add({
                  {"protocol", std::string(magic_enum::enum_name(protocol))},
                  {"tls_source", std::string(magic_enum::enum_name(tls_source))},
              })),
      conn_stats_bytes(prometheus::BuildCounter()
                           .Name("conn_stats_bytes")
                           .Help("Total bytes of data tracked by conn stats for this protocol.")
                           .Register(*registry)
                           .Add({
                               {"protocol", std::string(magic_enum::enum_name(protocol))},
                               {"tls_source", std::string(magic_enum::enum_name(tls_source))},
                           })) {}

namespace {

std::unordered_map<metrics_key, std::unique_ptr<SocketTracerMetrics>> g_protocol_metrics;

void ResetProtocolMetrics(traffic_protocol_t protocol, ssl_source_t tls_source) {
  std::pair<traffic_protocol_t, ssl_source_t> key = {protocol, tls_source};
  g_protocol_metrics.insert_or_assign(
      key, std::make_unique<SocketTracerMetrics>(&GetMetricsRegistry(), protocol, tls_source));
}
}  // namespace

SocketTracerMetrics& SocketTracerMetrics::GetProtocolMetrics(traffic_protocol_t protocol,
                                                             ssl_source_t tls_source) {
  std::pair<traffic_protocol_t, ssl_source_t> key = {protocol, tls_source};
  if (g_protocol_metrics.find(key) == g_protocol_metrics.end()) {
    ResetProtocolMetrics(protocol, tls_source);
  }
  return *g_protocol_metrics[key];
}

void SocketTracerMetrics::TestOnlyResetProtocolMetrics(traffic_protocol_t protocol,
                                                       ssl_source_t tls_source) {
  ResetProtocolMetrics(protocol, tls_source);
}

}  // namespace stirling
}  // namespace px
