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

#include "src/stirling/bpf_tools/bcc_bpf_intf/go_types.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

// Must be a power of two, otherwise masking will break.
#define HEADER_FIELD_STR_SIZE 128
#define MAX_DATA_SIZE 16384

// These checks are here for compatibility with BPF_LEN_CAP.
#ifdef __cplusplus
static_assert((MAX_DATA_SIZE & (MAX_DATA_SIZE - 1)) == 0, "MAX_DATA_SIZE must be a power of 2.");
#endif

struct header_field_t {
  uint32_t size;
  char msg[HEADER_FIELD_STR_SIZE];
};

enum http2_probe_type_t {
  k_probe_http2_operate_headers,
  k_probe_loopy_writer_write_header,
  k_probe_http2_client_operate_headers,
  k_probe_http2_server_operate_headers,
  k_probe_http_http2serverConn_processHeaders,
  k_probe_hpack_header_encoder,
  k_probe_http_http2writeResHeaders_write_frame,
  k_probe_http2_framer_check_frame_order,
  k_probe_http_http2Framer_check_frame_order,
  k_probe_http2_framer_write_data,
  k_probe_http_http2Framer_write_data,
  k_probe_type_other,
};

enum grpc_event_type_t {
  kEventUnknown,
  kHeaderEventRead,
  kHeaderEventWrite,
  kDataFrameEventRead,
  kDataFrameEventWrite
};

struct go_grpc_event_attr_t {
  enum grpc_event_type_t event_type;
  enum http2_probe_type_t probe_type;
  uint64_t timestamp_ns;
  struct conn_id_t conn_id;
  uint32_t stream_id;
  bool end_stream;
};

struct go_grpc_http2_header_event_t {
  // Must be the first member of this struct, to maintain common header with go_grpc_data_event_t.
  struct go_grpc_event_attr_t attr;

  struct header_field_t name;
  struct header_field_t value;
};

struct go_grpc_data_event_t {
  // Must be the first member of this struct, to maintain common header with
  // go_grpc_http2_header_event_t.
  struct go_grpc_event_attr_t attr;

  struct data_attr_t {
    // A 0-based position number for this event on the connection, in terms of byte position.
    // The position is for the first byte of this message.
    // Note that write/send have separate sequences than read/recv.
    uint64_t pos;
    // The size of the original message. We use this to truncate msg field to minimize the amount
    // of data being transferred.
    uint32_t data_size;
    // The amount of data actually being sent to user space. This may be less than msg_size if
    // data had to be truncated.
    uint32_t data_buf_size;
  } data_attr;

  char data[MAX_DATA_SIZE];
};
