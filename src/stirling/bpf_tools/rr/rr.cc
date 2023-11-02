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

#include "src/stirling/bpf_tools/rr/rr.h"

#include <fstream>

namespace px {
namespace stirling {
namespace bpf_tools {

void BPFRecorder::RecordBPFArrayTableGetValueEvent(const std::string& name, const int32_t idx,
                                                   const uint32_t data_size,
                                                   void const* const data) {
  auto event = events_proto_.add_event()->mutable_array_table_get_value_event();
  auto event_name = event->mutable_name();
  auto event_data = event->mutable_data();

  const std::string data_as_string(reinterpret_cast<const char*>(data), data_size);

  event->set_idx(idx);
  *event_name = name;
  *event_data = data_as_string;
}

void BPFRecorder::RecordBPFMapGetValueEvent(const std::string& name, const uint32_t key_size,
                                            void const* const key, const uint32_t val_size,
                                            void const* const val) {
  auto event = events_proto_.add_event()->mutable_map_get_value_event();
  auto event_name = event->mutable_name();
  auto event_key = event->mutable_key();
  auto event_val = event->mutable_value();

  const std::string key_as_string(reinterpret_cast<const char*>(key), key_size);
  const std::string val_as_string(reinterpret_cast<const char*>(val), val_size);

  *event_name = name;
  *event_key = key_as_string;
  *event_val = val_as_string;
}

void BPFRecorder::RecordBPFMapGetTableOfflineEvent(const std::string& name, const uint32_t size) {
  auto event = events_proto_.add_event()->mutable_map_get_table_offline_event();
  auto event_name = event->mutable_name();

  event->set_size(size);
  *event_name = name;
}

void BPFRecorder::RecordBPFMapCapacityEvent(const std::string& name, const int32_t n) {
  auto event = events_proto_.add_event()->mutable_map_capacity_event();
  auto event_name = event->mutable_name();

  event->set_capacity(n);
  *event_name = name;
}

void BPFRecorder::RecordBPFStackTableGetStackAddrEvent(const std::string& name,
                                                       const int32_t stack_id,
                                                       const std::vector<uintptr_t>& addrs) {
  auto event = events_proto_.add_event()->mutable_get_stack_addr_event();
  event->set_stack_id(stack_id);
  for (const auto& a : addrs) {
    event->add_addr(a);
  }
  auto event_name = event->mutable_name();
  *event_name = name;
}

void BPFRecorder::RecordBPFStackTableGetAddrSymbolEvent(const std::string& name,
                                                        const uint64_t addr, const uint32_t pid,
                                                        const std::string symbol) {
  auto event = events_proto_.add_event()->mutable_get_addr_symbol_event();
  auto event_name = event->mutable_name();
  auto event_symbol = event->mutable_symbol();
  event->set_addr(addr);
  event->set_pid(pid);
  *event_name = name;
  *event_symbol = symbol;
}

void BPFRecorder::WriteProto(const std::string& proto_buf_file_path) {
  if (!recording_written_) {
    LOG(INFO) << "Writing BPF events pb to file: " << proto_buf_file_path;

    std::fstream outfile(proto_buf_file_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!outfile.is_open()) {
      return;
    }

    if (!events_proto_.SerializeToOstream(&outfile)) {
      return;
    }
  }
  recording_written_ = true;
}

void BPFRecorder::RecordPerfBufferEvent(PerfBufferSpec* pb_spec, void const* const data,
                                        const int data_size) {
  auto event = events_proto_.add_event()->mutable_perf_buffer_event();
  auto pb_name = event->mutable_name();
  auto pb_data = event->mutable_data();

  const std::string data_as_string(static_cast<char const*>(data), data_size);

  *pb_name = pb_spec->name;
  *pb_data = data_as_string;
}

void RecordPerfBufferEvent(void* cb_cookie, void* data, int data_size) {
  PerfBufferSpec* pb_spec = static_cast<PerfBufferSpec*>(cb_cookie);
  BPFRecorder* recorder = pb_spec->recorder;
  ECHECK(recorder != nullptr);

  recorder->RecordPerfBufferEvent(pb_spec, data, data_size);
  pb_spec->probe_output_fn(pb_spec->cb_cookie, data, data_size);
}

void RecordPerfBufferLoss(void* cb_cookie, uint64_t lost) {
  PerfBufferSpec* pb_spec = static_cast<PerfBufferSpec*>(cb_cookie);
  pb_spec->probe_loss_fn(pb_spec->cb_cookie, lost);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Replay.
void BPFReplayer::ReplayPerfBufferEvents(const PerfBufferSpec& perf_buffer_spec) {
  if (PlaybackComplete()) {
    LOG_FIRST_N(INFO, 1) << "BPFReplayer::ReplayPerfBufferEvents(), plaback complete.";
    return;
  }

  const auto n_events = events_proto_.event_size();

  while (playback_event_idx_ < n_events) {
    const auto event = events_proto_.event(playback_event_idx_);
    if (!event.has_perf_buffer_event()) {
      // Return control to calling context. Replay will continue with a different eBPF data
      // type, e.g. map or array.
      break;
    }
    const auto perf_buffer_event = event.perf_buffer_event();
    const auto data = perf_buffer_event.data();
    const auto name = perf_buffer_event.name();
    if (name != perf_buffer_spec.name) {
      // Return control to calling context. Replay will continue with a different perf buffer.
      break;
    }
    const void* const_data = static_cast<const void*>(data.data());
    void* data_ptr = const_cast<void*>(static_cast<const void*>(const_data));
    auto f = perf_buffer_spec.probe_output_fn;
    f(perf_buffer_spec.cb_cookie, data_ptr, data.size());
    ++playback_event_idx_;
  }
}

Status BPFReplayer::ReplayArrayGetValue(const std::string& name, const int32_t idx,
                                        const uint32_t data_size, void* value) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_array_table_get_value_event()) {
    return error::Internal("Array table event not available.");
  }

  const auto event = event_wrapper.array_table_get_value_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched eBPF array name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  if (idx != event.idx()) {
    const char* const msg = "Mismatched array index. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.idx(), idx));
  }
  if (data_size != event.data().size()) {
    const char* const msg = "Mismatched data size. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.data().size(), data_size));
  }

  memcpy(value, event.data().data(), data_size);
  ++playback_event_idx_;

  return Status::OK();
}

Status BPFReplayer::ReplayMapGetValue(const std::string& name, const uint32_t key_size,
                                      void const* const key, const uint32_t val_size, void* value) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_map_get_value_event()) {
    return error::Internal("Map event not available.");
  }

  const auto event = event_wrapper.map_get_value_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched eBPF map name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  if (key_size != event.key().size()) {
    const char* const msg = "Mismatched key size. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.key().size(), key_size));
  }
  if (val_size != event.value().size()) {
    const char* const msg = "Mismatched value size. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.value().size(), val_size));
  }
  if (0 != memcmp(key, event.key().data(), key_size)) {
    return error::Internal("Mismatched key.");
  }

  memcpy(value, event.value().data(), val_size);
  ++playback_event_idx_;

  return Status::OK();
}

Status BPFReplayer::ReplayMapGetKeyAndValue(const std::string& name, const uint32_t key_size,
                                            void* key, const uint32_t val_size, void* val) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_map_get_value_event()) {
    return error::Internal("Map event not available.");
  }

  const auto event = event_wrapper.map_get_value_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched eBPF map name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  if (key_size != event.key().size()) {
    const char* const msg = "Mismatched key size. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.key().size(), key_size));
  }
  if (val_size != event.value().size()) {
    const char* const msg = "Mismatched value size. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.value().size(), val_size));
  }
  memcpy(key, event.key().data(), key_size);
  memcpy(val, event.value().data(), val_size);
  ++playback_event_idx_;

  return Status::OK();
}

StatusOr<int32_t> BPFReplayer::ReplayBPFMapCapacityEvent(const std::string& name) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_map_capacity_event()) {
    return error::Internal("Map event not available.");
  }

  const auto event = event_wrapper.map_capacity_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched eBPF map name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  ++playback_event_idx_;

  return event.capacity();
}

StatusOr<int32_t> BPFReplayer::ReplayBPFMapGetTableOfflineEvent(const std::string& name) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_map_get_table_offline_event()) {
    return error::Internal("Map event not available.");
  }

  const auto event = event_wrapper.map_get_table_offline_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched eBPF map name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  ++playback_event_idx_;

  return event.size();
}

StatusOr<std::vector<uintptr_t>> BPFReplayer::ReplayBPFStackTableGetStackAddrEvent(
    const std::string& name, const int32_t stack_id) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_get_stack_addr_event()) {
    return error::Internal("Map event not available.");
  }

  const auto event = event_wrapper.get_stack_addr_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched eBPF stack table name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  if (stack_id != event.stack_id()) {
    const char* const msg = "Mismatched stack id. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.stack_id(), stack_id));
  }
  ++playback_event_idx_;

  std::vector<uintptr_t> addrs;

  for (int i = 0; i < event.addr_size(); ++i) {
    const uint64_t addr = event.addr(i);
    addrs.push_back(addr);
  }
  return addrs;
}

StatusOr<std::string> BPFReplayer::ReplayBPFStackTableGetAddrSymbolEvent(const std::string& name,
                                                                         const uint64_t addr,
                                                                         const uint32_t pid) {
  if (PlaybackComplete()) {
    return error::Internal("Playback complete.");
  }

  const auto event_wrapper = events_proto_.event(playback_event_idx_);
  if (!event_wrapper.has_get_addr_symbol_event()) {
    return error::Internal("Stack table get addr symbol event not available.");
  }

  const auto event = event_wrapper.get_addr_symbol_event();

  if (name != event.name()) {
    const char* const msg = "Mismatched stack table name. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.name(), name));
  }
  if (addr != event.addr()) {
    const char* const msg = "Mismatched addr. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.addr(), addr));
  }
  if (pid != event.pid()) {
    const char* const msg = "Mismatched pid. Expected: $0, requested: $1.";
    return error::Internal(absl::Substitute(msg, event.pid(), pid));
  }
  ++playback_event_idx_;

  return event.symbol();
}

Status BPFReplayer::OpenReplayProtobuf(const std::string& replay_events_pb_file_path) {
  LOG(INFO) << absl::Substitute("replay_events_pb_file_path: $0.", replay_events_pb_file_path);
  std::fstream input(replay_events_pb_file_path, std::ios::in | std::ios::binary);

  if (!input) {
    return error::Internal("Could not open file: $0.", replay_events_pb_file_path);
  } else if (!events_proto_.ParseFromIstream(&input)) {
    return error::Internal("Could not parse file: $0.", replay_events_pb_file_path);
  }
  LOG(INFO) << absl::Substitute("events_proto_.size(): $0.", events_proto_.event_size());
  return Status::OK();
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
