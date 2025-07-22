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

#include <map>
#include <memory>
#include <string>
#include <tuple>
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
                                         traffic_protocol_t protocol, ssl_source_t tls_source,
                                         chunk_t incomplete_chunk, bool lazy_parsing_enabled)
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
                           })),
      unparseable_bytes_before_gap(
          prometheus::BuildCounter()
              .Name("unparseable_bytes_before_gap")
              .Help("Records the number of bytes that were unparseable before the gap of an "
                    "incomplete chunk i.e. rendered unparseable because of the presence of a"
                    " gap (even if filled with null bytes) in a contiguous section of the "
                    "data stream buffer. ")
              .Register(*registry)
              .Add({
                  {"protocol", std::string(magic_enum::enum_name(protocol))},
                  {"tls_source", std::string(magic_enum::enum_name(tls_source))},
                  {"incomplete_reason", std::string(magic_enum::enum_name(incomplete_chunk))},
                  {"lazy_parsing_enabled", lazy_parsing_enabled ? "true" : "false"},
              })) {}

namespace {

using metrics_key = std::tuple<traffic_protocol_t, ssl_source_t, chunk_t, bool>;
std::map<metrics_key, std::unique_ptr<SocketTracerMetrics>> g_protocol_metrics;

void ResetProtocolMetrics(traffic_protocol_t protocol, ssl_source_t tls_source,
                          chunk_t incomplete_chunk = chunk_t::kFullyFormed,
                          bool lazy_parsing_enabled = false) {
  metrics_key key = std::make_tuple(protocol, tls_source, incomplete_chunk, lazy_parsing_enabled);
  g_protocol_metrics[key] = std::make_unique<SocketTracerMetrics>(
      &GetMetricsRegistry(), protocol, tls_source, incomplete_chunk, lazy_parsing_enabled);
}
}  // namespace

SocketTracerMetrics& SocketTracerMetrics::GetProtocolMetrics(traffic_protocol_t protocol,
                                                             ssl_source_t tls_source,
                                                             chunk_t incomplete_chunk,
                                                             bool lazy_parsing_enabled) {
  metrics_key key = std::make_tuple(protocol, tls_source, incomplete_chunk, lazy_parsing_enabled);
  if (g_protocol_metrics.find(key) == g_protocol_metrics.end()) {
    ResetProtocolMetrics(protocol, tls_source, incomplete_chunk, lazy_parsing_enabled);
  }
  return *g_protocol_metrics[key];
}

void SocketTracerMetrics::TestOnlyResetProtocolMetrics(traffic_protocol_t protocol,
                                                       ssl_source_t tls_source,
                                                       chunk_t incomplete_chunk,
                                                       bool lazy_parsing_enabled) {
  ResetProtocolMetrics(protocol, tls_source, incomplete_chunk, lazy_parsing_enabled);
}

}  // namespace stirling
}  // namespace px
