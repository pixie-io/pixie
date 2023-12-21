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
#include <utility>

#include "src/stirling/source_connectors/socket_tracer/testing/benchmark_data_gen/data_gen.h"

namespace px {
namespace stirling {
namespace testing {

namespace {
// For the purposes of benchmarking PID doesn't matter too much, since we can just split things by
// different FDs.
constexpr uint32_t kPID = 12345;
constexpr uint64_t kUPIDStartTimeTicks = 1;

socket_control_event_t OpenConnEvent(uint64_t ts, int32_t fd, endpoint_role_t role,
                                     uint32_t pid = kPID, uint64_t tsid = 0) {
  struct socket_control_event_t conn_event {};
  conn_event.type = kConnOpen;
  conn_event.timestamp_ns = ts;
  conn_event.conn_id.upid.pid = pid;
  conn_event.conn_id.fd = fd;
  conn_event.conn_id.tsid = tsid;
  conn_event.conn_id.upid.start_time_ticks = kUPIDStartTimeTicks;
  conn_event.open.raddr.sa.sa_family = AF_INET;
  conn_event.open.laddr.sa.sa_family = AF_INET;
  conn_event.open.role = role;
  return conn_event;
}

socket_data_event_t DataEvent(traffic_direction_t direction, traffic_protocol_t protocol,
                              endpoint_role_t role, uint64_t ts, int32_t fd, uint64_t pos,
                              std::string_view msg, uint32_t pid = kPID, uint64_t tsid = 0) {
  socket_data_event_t event = {};
  event.attr.direction = direction;
  event.attr.protocol = protocol;
  event.attr.role = role;
  event.attr.timestamp_ns = ts;
  event.attr.conn_id.upid.pid = pid;
  event.attr.conn_id.fd = fd;
  event.attr.conn_id.tsid = tsid;
  event.attr.conn_id.upid.start_time_ticks = kUPIDStartTimeTicks;
  event.attr.pos = pos;
  event.attr.msg_size = msg.size();
  event.attr.msg_buf_size = msg.size();
  msg.copy(event.msg, msg.size());
  return event;
}

void EmitEventInChunks(traffic_direction_t direction, traffic_protocol_t protocol,
                       endpoint_role_t role, uint64_t ts, int32_t conn_id, uint64_t pos,
                       std::string_view msg, std::vector<socket_data_event_t>* data_events) {
  auto remaining_bytes = msg.size();
  while (remaining_bytes > 0) {
    auto chunk_size = (remaining_bytes < MAX_MSG_SIZE) ? remaining_bytes : MAX_MSG_SIZE;
    auto pos_in_msg = msg.size() - remaining_bytes;
    auto chunk_msg = msg.substr(pos_in_msg, chunk_size);
    data_events->push_back(
        DataEvent(direction, protocol, role, ts, conn_id, pos + pos_in_msg, chunk_msg));
    remaining_bytes -= chunk_size;
  }
}

}  // namespace

BenchmarkDataGenerationOutput GenerateBenchmarkData(const BenchmarkDataGenerationSpec& spec) {
  BenchmarkDataGenerationOutput output;
  std::map<int32_t, std::unique_ptr<RecordGenerator>> conn_to_rec_gen;
  std::map<std::pair<int32_t, traffic_direction_t>, std::unique_ptr<PosGenerator>>
      conn_dir_to_pos_gen;
  // Create conns. For now, all conns have the same PID, so they are uniquely refered to by FD.
  int32_t last_conn_id = 0;
  // TODO(james): right now ts is incremented every message, should probably have timestamp jump
  // when there are gaps in pos.
  uint64_t ts = 1;
  for (size_t i = 0; i < spec.num_conns; ++i) {
    output.control_events.push_back(OpenConnEvent(ts, last_conn_id, spec.role));
    // A PosGenerator is created for each direction of each connection.
    conn_dir_to_pos_gen.emplace(std::make_pair(last_conn_id, kEgress), spec.pos_gen_func());
    conn_dir_to_pos_gen.emplace(std::make_pair(last_conn_id, kIngress), spec.pos_gen_func());
    // Creating a new RecordGenerator per connection is a little unnecessary at the moment, since
    // none of the current RecordGenerator's require state. But this allows for a future with more
    // complicated RecordGenerator's, for example, a RecordGenerator that interleaves different
    // kinds of records on the same connection.
    conn_to_rec_gen.emplace(last_conn_id, spec.rec_gen_func());
    last_conn_id++;
  }
  // Build data events for each conn.
  for (size_t iter_idx = 0; iter_idx < spec.num_poll_iterations; ++iter_idx) {
    std::vector<socket_data_event_t> data_events;
    for (size_t i = 0; i < spec.records_per_conn; ++i) {
      for (int32_t conn_id = 0; conn_id < last_conn_id; conn_id++) {
        auto record = conn_to_rec_gen[conn_id]->Next(conn_id);
        auto recv_pos =
            conn_dir_to_pos_gen[std::make_pair(conn_id, kIngress)]->NextPos(record.recv_bytes);
        auto send_pos =
            conn_dir_to_pos_gen[std::make_pair(conn_id, kEgress)]->NextPos(record.send_bytes);
        output.data_size_bytes += record.recv_bytes;
        output.data_size_bytes += record.send_bytes;

        for (const auto& [direction, msg] : record.frames) {
          size_t* pos = &recv_pos;
          if (direction == kEgress) {
            pos = &send_pos;
          }
          EmitEventInChunks(direction, spec.protocol, spec.role, ts++, conn_id, *pos, msg,
                            &data_events);
          *pos += msg.size();
        }
      }
    }
    output.per_iter_data_events.push_back(data_events);
    for (int32_t conn_id = 0; conn_id < last_conn_id; conn_id++) {
      conn_dir_to_pos_gen[std::make_pair(conn_id, kIngress)]->NextPollIteration();
      conn_dir_to_pos_gen[std::make_pair(conn_id, kEgress)]->NextPollIteration();
    }
  }
  return output;
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
