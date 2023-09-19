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
  if (PlabackComplete()) {
    LOG_FIRST_N(INFO, 1) << "BPFReplayer::ReplayPerfBufferEvents(), plaback complete.";
    return;
  }

  const auto n_events = events_proto_.event_size();

  while (playback_event_idx_ < n_events) {
    const auto event = events_proto_.event(playback_event_idx_);
    if (!event.has_perf_buffer_event()) {
      break;
    }
    const auto perf_buffer_event = event.perf_buffer_event();
    const auto data = perf_buffer_event.data();
    const auto name = perf_buffer_event.name();
    if (name != perf_buffer_spec.name) {
      break;
    }
    const void* const_data = static_cast<const void*>(data.data());
    void* data_ptr = const_cast<void*>(static_cast<const void*>(const_data));
    auto f = perf_buffer_spec.probe_output_fn;
    f(perf_buffer_spec.cb_cookie, data_ptr, data.size());
    ++playback_event_idx_;
  }
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
