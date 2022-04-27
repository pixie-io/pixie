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
                                         traffic_protocol_t protocol)
    : data_loss_bytes(
          prometheus::BuildCounter()
              .Name("data_loss_bytes")
              .Help("Total bytes of data loss for this protocol. Measured by bytes that weren't "
                    "successfully parsed.")
              .Register(*registry)
              .Add({{"protocol", std::string(magic_enum::enum_name(protocol))}})) {}

namespace {
std::unordered_map<traffic_protocol_t, std::unique_ptr<SocketTracerMetrics>> g_protocol_metrics;

void ResetProtocolMetrics(traffic_protocol_t protocol) {
  g_protocol_metrics.insert_or_assign(
      protocol, std::make_unique<SocketTracerMetrics>(&GetMetricsRegistry(), protocol));
}
}  // namespace

SocketTracerMetrics& SocketTracerMetrics::GetProtocolMetrics(traffic_protocol_t protocol) {
  if (g_protocol_metrics.find(protocol) == g_protocol_metrics.end()) {
    ResetProtocolMetrics(protocol);
  }
  return *g_protocol_metrics[protocol];
}

void SocketTracerMetrics::TestOnlyResetProtocolMetrics(traffic_protocol_t protocol) {
  ResetProtocolMetrics(protocol);
}

}  // namespace stirling
}  // namespace px
