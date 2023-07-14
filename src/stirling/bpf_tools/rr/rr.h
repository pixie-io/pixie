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

#include <filesystem>
#include <string>

#include "src/stirling/bpf_tools/probe_specs/probe_specs.h"
#include "src/stirling/bpf_tools/rr/rr.pb.h"

namespace px {
namespace stirling {
namespace bpf_tools {

class RRSingleton {
 public:
  static RRSingleton& GetInstance() {
    static RRSingleton instance;
    return instance;
  }

  void ReplayPerfBufferEvents(const PerfBufferSpec& perf_buffer_spec);

  void RecordBPFArrayTableGetValueEvent(const std::string& name, const int32_t idx,
                                        const uint32_t data_size, void const* const data);
  Status ReplayArrayGetValue(const std::string& name, const int32_t idx, void* data,
                             const uint32_t data_size);

  bool recording() const { return recording_; }
  bool replaying() const { return replaying_; }
  bool PlabackComplete() const { return playback_event_idx_ >= events_proto_.event_size(); }

  void SetRecording();
  void SetReplaying(const std::string& proto_buf_file_path);

  void WriteProto(const std::string& proto_buf_file_path);

  ::px::stirling::rr::BPFEvents& events_proto() { return events_proto_; }

 private:
  RRSingleton() {}

  int64_t playback_event_idx_ = 0;
  bool recording_ = false;
  bool replaying_ = false;
  bool recording_written_ = false;
  ::px::stirling::rr::BPFEvents events_proto_;
};

void RecordPerfBufferEvent(void* cb_cookie, void* data, int data_size);
void RecordPerfBufferLoss(void* cb_cookie, uint64_t lost);

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
