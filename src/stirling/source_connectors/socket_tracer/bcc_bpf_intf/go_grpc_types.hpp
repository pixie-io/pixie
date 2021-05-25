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

#include <algorithm>
#include <string>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.h"

// perf_submit() uses PERF_RECORD_SAMPLE with PERF_SAMPLE_RAW, which has the following structure.
//      struct {
//        struct perf_event_header {
//          __u32   type;
//          __u16   misc;
//          __u16   size;
//        } header;
//        u32    size;        /* if PERF_SAMPLE_RAW */
//        char  data[size];   /* if PERF_SAMPLE_RAW */
//      };
// The entire struct as a whole is 12-bytes + data[size].
// This means the data member is 4-bytes aligned.
//
// If data is reinterpreted as any struct with 8-byte members,
// access to the struct's members can cause misalignment errors.
// For this reason, the data needs to be copied to an 8-byte boundary before being used.
//
// To perform the copies, we get the pointers and copy the data in safer ways.
//  - memcpy is always safe since it treats data as bytes, and only requires 1-byte alignment.
//      - This is used to copy the bulk of the struct data.
//  - reinterpret_cast<uint32_t*> and copy is also safe, because uint32_t only requires 4-byte
//  alignment.
//      - This is used to copy uint32_t lengths.

namespace px {
namespace stirling {

inline std::string ToString(const struct go_grpc_event_attr_t& attr) {
  return absl::Substitute(
      "[event_type=$0 probe_type=$1 timestamp_ns=$2 conn_id=$3 stream_id=$4 end_stream=$5] ",
      magic_enum::enum_name(attr.event_type), magic_enum::enum_name(attr.probe_type), attr.timestamp_ns,
      ToString(attr.conn_id), attr.stream_id, attr.end_stream);
}

struct HTTP2DataEvent {
  HTTP2DataEvent() : attr{}, payload{} {}
  explicit HTTP2DataEvent(const void* data) {
    auto data_ptr = static_cast<const char*>(data);

    auto attr_ptr = data_ptr + offsetof(go_grpc_data_event_t, attr);
    auto data_attr_ptr = data_ptr + offsetof(go_grpc_data_event_t, data_attr);
    auto payload_ptr = data_ptr + offsetof(go_grpc_data_event_t, data);

    memcpy(&attr, attr_ptr, sizeof(go_grpc_event_attr_t));
    memcpy(&data_attr, data_attr_ptr, sizeof(go_grpc_data_event_t::data_attr_t));

    payload = std::string_view(payload_ptr, data_attr.data_buf_size);
  }

  std::string ToString() const {
    return absl::Substitute("[attr=$0 pos=$1 data_size=$2 data_buf_size=$3 payload=$4]", ::px::stirling::ToString(attr),
                            data_attr.pos, data_attr.data_size, payload.size(),
                            BytesToString<bytes_format::HexAsciiMix>(payload));
  }

  go_grpc_event_attr_t attr;
  go_grpc_data_event_t::data_attr_t data_attr;
  std::string_view payload;
};

struct HTTP2HeaderEvent {
  HTTP2HeaderEvent() : attr{} {}
  explicit HTTP2HeaderEvent(const void* data) {
    auto data_ptr = static_cast<const char*>(data);

    // Pointers into relevant sub-fields within the go_grpc_http2_header_event_t struct.
    auto attr_ptr = data_ptr + offsetof(go_grpc_http2_header_event_t, attr);
    auto name_data_ptr = data_ptr + offsetof(go_grpc_http2_header_event_t, name.msg);
    auto name_len_ptr = data_ptr + offsetof(go_grpc_http2_header_event_t, name.size);
    auto value_data_ptr = data_ptr + offsetof(go_grpc_http2_header_event_t, value.msg);
    auto value_len_ptr = data_ptr + offsetof(go_grpc_http2_header_event_t, value.size);

    // Copy attr sub-struct via memcpy as char -- requires 1-byte alignment.
    memcpy(&attr, attr_ptr, sizeof(go_grpc_event_attr_t));

    // Copy name length (uint32_t) -- requires 4-byte alignment.
    uint32_t name_len = *reinterpret_cast<const uint32_t*>(name_len_ptr);

    // Copy name string (char) -- requires 1-byte alignment.
    name = std::string_view(name_data_ptr,
                std::min<uint32_t>(name_len, sizeof(go_grpc_http2_header_event_t::name)));

    // Copy value length (uint32_t) -- requires 4-byte alignment.
    uint32_t value_len = *reinterpret_cast<const uint32_t*>(value_len_ptr);

    // Copy value string (char) -- requires 1-byte alignment.
    value = std::string_view(value_data_ptr,
                 std::min<uint32_t>(value_len, sizeof(go_grpc_http2_header_event_t::value)));
  }

  std::string ToString() const {
    return absl::Substitute("[attr=$0] [name=$1] [value=$2]", ::px::stirling::ToString(attr), name,
                            value);
  }

  go_grpc_event_attr_t attr;
  std::string_view name;
  std::string_view value;
};

}  // namespace stirling
}  // namespace px
