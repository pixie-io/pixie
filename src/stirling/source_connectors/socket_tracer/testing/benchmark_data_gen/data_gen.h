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
#include <set>
#include <string>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/testing/benchmark_data_gen/generators.h"

namespace px {
namespace stirling {
namespace testing {

using RecordGenFunc = std::function<std::unique_ptr<RecordGenerator>()>;
using PosGenFunc = std::function<std::unique_ptr<PosGenerator>()>;

struct BenchmarkDataGenerationSpec {
  // Number of simultaneous connections to generate events for.
  size_t num_conns;

  // Number of polling iterations to emulate. Each polling iteration consists of "polling the ebpf
  // buffers" (the benchmark emulates polling ebpf buffers by pushing data to SocketTraceConnector)
  // and then a call to SocketTraceConnector::TransferData.
  size_t num_poll_iterations;

  // Number of records to generate for each connection for each poll iteration.
  size_t records_per_conn;

  // Traffic protocol for traffic that the RecordGenerator outputs.
  traffic_protocol_t protocol;

  // Traffic role for the traffic that the RecordGenerator outputs.
  endpoint_role_t role;

  // Function to create a RecordGenerator for each connection.
  RecordGenFunc rec_gen_func;

  // Function to create a PosGenerator for each direction of each connection.
  PosGenFunc pos_gen_func;
};

struct BenchmarkDataGenerationOutput {
  // Control events necessary for starting each connection. For now its only possible to start
  // connections, so connections live for the duration of the benchmark.
  std::vector<socket_control_event_t> control_events;

  // Data events per iteration. Each "iteration" of data represents one SocketTraceConnector polling
  // iteration.
  std::vector<std::vector<socket_data_event_t>> per_iter_data_events;

  // Total size of all messages in per_iter_data_event. This does not include the size of
  // socket_data_event_t::attr.
  uint64_t data_size_bytes = 0;
};

BenchmarkDataGenerationOutput GenerateBenchmarkData(const BenchmarkDataGenerationSpec& spec);

}  // namespace testing
}  // namespace stirling
}  // namespace px
