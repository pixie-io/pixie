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

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/testing/clock.h"

namespace px {
namespace stirling {
namespace testing {

class StreamEventGenerator {
 public:
  StreamEventGenerator(Clock* clock, conn_id_t conn_id, uint32_t stream_id)
      : clock_(clock), conn_id_(conn_id), stream_id_(stream_id) {}

  template <grpc_event_type_t TType>
  std::unique_ptr<HTTP2DataEvent> GenDataFrame(std::string_view body, bool end_stream = false) {
    auto frame = std::make_unique<HTTP2DataEvent>();
    frame->attr.conn_id = conn_id_;
    frame->attr.event_type = TType;
    frame->attr.timestamp_ns = clock_->now();
    frame->attr.stream_id = stream_id_;
    frame->attr.end_stream = end_stream;
    frame->data_attr.pos = pos_;
    pos_ += body.length();
    frame->data_attr.data_size = body.length();
    frame->payload = body;
    return frame;
  }

  template <grpc_event_type_t TType>
  std::unique_ptr<HTTP2HeaderEvent> GenHeader(std::string_view name, std::string_view value) {
    auto hdr = std::make_unique<HTTP2HeaderEvent>();
    hdr->attr.conn_id = conn_id_;
    hdr->attr.event_type = TType;
    hdr->attr.timestamp_ns = clock_->now();
    hdr->attr.stream_id = stream_id_;
    hdr->attr.end_stream = false;
    hdr->name = name;
    hdr->value = value;
    return hdr;
  }

  template <grpc_event_type_t TType>
  std::unique_ptr<HTTP2HeaderEvent> GenEndStreamHeader() {
    auto hdr = std::make_unique<HTTP2HeaderEvent>();
    hdr->attr.conn_id = conn_id_;
    hdr->attr.event_type = TType;
    hdr->attr.timestamp_ns = clock_->now();
    hdr->attr.stream_id = stream_id_;
    hdr->attr.end_stream = true;
    hdr->name = "";
    hdr->value = "";
    return hdr;
  }

 private:
  // Used to track the position for the next data frame.
  size_t pos_ = 0;
  Clock* clock_;
  conn_id_t conn_id_;
  uint32_t stream_id_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
