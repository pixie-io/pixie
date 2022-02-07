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
#include <utility>

#include "src/stirling/core/connector_context.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

namespace px {
namespace stirling {
// Note that this has to be in the same namespace as `SocketTraceConnector` for the friend class
// to work. Hence why its not in the testing namespace.

// A wrapper around SocketTraceConnector that exposes private methods that would normally be called
// from BPF event callbacks. Useful for simulating the arrival of BPF events in a controlled way.
class SocketTraceConnectorFriend : public SocketTraceConnector {
 public:
  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SocketTraceConnectorFriend(name));
  }
  explicit SocketTraceConnectorFriend(std::string_view name) : SocketTraceConnector(name) {}

  void AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
    SocketTraceConnector::AcceptDataEvent(std::move(event));
  }
  void AcceptControlEvent(socket_control_event_t event) {
    SocketTraceConnector::AcceptControlEvent(event);
  }
  void AcceptConnStatsEvent(conn_stats_event_t event) {
    SocketTraceConnector::AcceptConnStatsEvent(event);
  }
  void AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
    SocketTraceConnector::AcceptHTTP2Header(std::move(event));
  }
  void AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
    SocketTraceConnector::AcceptHTTP2Data(std::move(event));
  }
  void HandleDataEvent(socket_data_event_t* data, int data_size) {
    SocketTraceConnector::HandleDataEvent(this, data, data_size);
  }
  void HandleControlEvent(socket_control_event_t* data, int data_size) {
    SocketTraceConnector::HandleControlEvent(this, data, data_size);
  }
  void HandleConnStatsEvent(conn_stats_event_t* data, int data_size) {
    SocketTraceConnector::HandleConnStatsEvent(this, data, data_size);
  }
  void HandleMMapEvent(upid_t* data, int data_size) {
    SocketTraceConnector::HandleMMapEvent(this, data, data_size);
  }
  void HandleHTTP2HeaderEvent(go_grpc_http2_header_event_t* data, int data_size) {
    SocketTraceConnector::HandleHTTP2Event(this, data, data_size);
  }
  void HandleHTTP2Data(go_grpc_data_event_t* data, int data_size) {
    SocketTraceConnector::HandleHTTP2Event(this, data, data_size);
  }
};

}  // namespace stirling
}  // namespace px
