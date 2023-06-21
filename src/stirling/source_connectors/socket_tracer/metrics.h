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
#include <prometheus/registry.h>

#include "src/common/metrics/metrics.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

namespace px {
namespace stirling {

struct SocketTracerMetrics {
  SocketTracerMetrics(prometheus::Registry* registry, traffic_protocol_t protocol,
                      ssl_source_t tls_source);
  prometheus::Counter& data_loss_bytes;
  prometheus::Counter& conn_stats_bytes;

  static SocketTracerMetrics& GetProtocolMetrics(traffic_protocol_t protocol,
                                                 ssl_source_t tls_source);

  static void TestOnlyResetProtocolMetrics(traffic_protocol_t protocol, ssl_source_t tls_source);
};

}  // namespace stirling
}  // namespace px
