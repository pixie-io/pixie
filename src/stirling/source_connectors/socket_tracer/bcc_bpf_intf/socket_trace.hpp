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
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <absl/hash/hash.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.hpp"

// This header defines the C++ counterparts of the BPF data structures.
// The file name is kept identical to its BPF counterpart as well.

inline std::string ToString(const socket_data_event_t::attr_t& attr) {
  // Since absl::Substitute can handle up to 10 arguments after the format string,
  // we concatenate the incomplete_chunk string separately.
  std::string base_str = absl::Substitute(
      "[ts=$0 conn_id=$1 protocol=$2 role=$3 dir=$4 ssl=$5 source_fn=$6 pos=$7 size=$8 "
      "buf_size=$9",
      attr.timestamp_ns, ToString(attr.conn_id), magic_enum::enum_name(attr.protocol),
      magic_enum::enum_name(attr.role), magic_enum::enum_name(attr.direction), attr.ssl,
      magic_enum::enum_name(attr.source_fn), attr.pos, attr.msg_size, attr.msg_buf_size);

  // Second part: Continue with the next set of attributes.
  std::string second_part = absl::Substitute(
      " bytes_missed=$0 incomplete_chunk=$1]",
      attr.bytes_missed, magic_enum::enum_name(attr.incomplete_chunk));

  // Concatenate both parts and return.
  return absl::StrCat(base_str, " ", second_part);
}

inline std::string ToString(const close_event_t& event) {
  return absl::Substitute("[wr_bytes=$0 rd_bytes=$1]", event.wr_bytes, event.rd_bytes);
}

inline std::string ToString(const conn_event_t& event) {
  return absl::Substitute("[addr=$0]",
                          ::px::ToString(reinterpret_cast<const struct sockaddr*>(&event.addr)));
}

inline std::string ToString(const socket_control_event_t& event) {
  return absl::Substitute("[type=$0 ts=$1 conn_id=$2 source_fn=$3 $4]", magic_enum::enum_name(event.type),
                          event.timestamp_ns, ToString(event.conn_id),
                          magic_enum::enum_name(event.source_fn),
                          event.type == kConnOpen ? ToString(event.open) : ToString(event.close));
}

namespace px {
namespace stirling {

/**
 * @brief A C++ friendly counterpart to socket_data_event_t. The memory buffer is managed through a
 * std::string, instead of the "struct hack" in C: http://c-faq.com/struct/structhack.html.
 *
 * Note that socket_data_event_t::msg cannot go beyond the fixed size, because of it's used inside
 * BPF. That's a minor difference to the ordinary struct hack.
 */
struct SocketDataEvent {
  SocketDataEvent() : attr{}, msg{} {}
  explicit SocketDataEvent(const void* data) {
    auto data_ptr = static_cast<const char*>(data);

    // Pointers into relevant sub-fields within the socket_data_event_t struct.
    auto attr_ptr = data_ptr + offsetof(socket_data_event_t, attr);
    auto msg_ptr = data_ptr + offsetof(socket_data_event_t, msg);

    // The perf buffer's payload is not 8-byte aligned.
    // Each submission has 2 parts: a 4 bytes size, and the subsequent payload.
    // To avoid unaligned accesses, we must copy anything that has an 8 byte member.
    memcpy(&attr, attr_ptr, sizeof(socket_data_event_t::attr_t));

    // Strings only require 1-byte alignment, so safe to use string_view here instead of copy.
    msg = std::string_view(msg_ptr, attr.msg_buf_size);
  }

  // The servers of certain protocols (e.g. Kafka) read the length headers of frames separately
  // from the payload. In these cases, the protocol inference misses the header of the first frame.
  // This header is encoded in the attributes instead.
  // We account for this with a separate header event.
  std::unique_ptr<SocketDataEvent> ExtractHeaderEvent() {
    std::unique_ptr<SocketDataEvent> header_event_ptr;

    if (attr.prepend_length_header) {
      VLOG(1) << "Adding header event";

      constexpr int kHeaderBufSize = 4;

      header_event_ptr = std::make_unique<SocketDataEvent>();
      header_event_ptr->attr = attr;
      header_event_ptr->attr.pos = attr.pos - kHeaderBufSize;
      header_event_ptr->attr.msg_buf_size = kHeaderBufSize;
      header_event_ptr->attr.msg_size = kHeaderBufSize;
      header_event_ptr->attr.incomplete_chunk = kHeaderEvent;
      header_event_ptr->attr.bytes_missed = 0;

      // Take the length_header from the original, fix byte ordering, and place
      // into length_header of the header_event.
      char header[kHeaderBufSize];
      px::utils::IntToLEndianBytes(attr.length_header, header);
      memcpy(&header_event_ptr->attr.length_header, header, kHeaderBufSize);

      header_event_ptr->msg = std::string_view(reinterpret_cast<char*>(&header_event_ptr->attr.length_header), kHeaderBufSize);

      // We've extracted the header event, so remove these attributes from the original event.
      attr.prepend_length_header = false;
      attr.length_header = 0;
    }

    return header_event_ptr;
  }

  // For events that which couldn't transfer all its data, we have two options:
  //  1) A missing event.
  //  2) A filler event.
  // A desired filler event is indicated by a bytes_missed > 0 when creating the BPF event.
  //
  // A filler event is used in particular for sendfile data.
  // We need a better long-term solution for this,
  // since we aren't able to directly trace the data.
  std::unique_ptr<SocketDataEvent> ExtractFillerEvent() {
    std::unique_ptr<SocketDataEvent> filler_event_ptr;

    DCHECK_GE(attr.msg_size, attr.msg_buf_size);

    // Note that msg_size - msg_buf_size != bytes_missed in the case where we exceed LOOP_LIMIT
    // in perf_submit_iovecs, because one call to perf_submit_buf takes only the size of the current
    // iovec into account, ommitting the rest of the iovecs which could not be submitted.
    // As a result, we need to use bytes_missed to determine the size of the filler event.

    // For kernels < 5.1, we cannot track the bytes missed in socket_trace.c properly and thus
    // preserve the previous behavior of encoding the bytes missed via the msg_size.
    // If our loop and chunk limits are at most 42 and 4, then we know that we can
    // stay below the verifier instruction limit for kernels < 5.1.
    if (LOOP_LIMIT <= 42 && CHUNK_LIMIT <= 4) {
      if (attr.msg_size > attr.msg_buf_size) {
          DCHECK_EQ(attr.bytes_missed, 0);
          attr.bytes_missed = attr.msg_size - attr.msg_buf_size;
      }
    }
    if (attr.bytes_missed > 0) {
      VLOG(1) << absl::Substitute("Adding filler event for incomplete_chunk: $0, bytes_missed: $1", magic_enum::enum_name(attr.incomplete_chunk), attr.bytes_missed);

      // Limit the size so we don't have huge allocations.
      constexpr uint32_t kMaxFilledSizeBytes = 1 * 1024 * 1024;
      static char kZeros[kMaxFilledSizeBytes] = {0};

      filler_event_ptr = std::make_unique<SocketDataEvent>();
      filler_event_ptr->attr = attr;
      size_t filler_size = attr.bytes_missed;
      if (filler_size > kMaxFilledSizeBytes) {
        VLOG(1) << absl::Substitute("Truncating filler event: $0->$1", filler_size,
                                    kMaxFilledSizeBytes);
        filler_size = kMaxFilledSizeBytes;
        // incomplete even after filler (bytes_missed > 1MB)
        filler_event_ptr->attr.incomplete_chunk = kIncompleteFiller;
        filler_event_ptr->attr.bytes_missed -= kMaxFilledSizeBytes;
      } else {
        // We encode the filler size in bytes_missed for filler events which completely plug a gap (chunk_t kFiller) in our metrics.
        // (In reality, bytes missed is 0 since filler plugs the gap.)
        // In all other circumstances bytes_missed represents the size of the gap
        filler_event_ptr->attr.incomplete_chunk = kFiller;
      }
      filler_event_ptr->attr.pos = attr.pos + attr.msg_buf_size;
      filler_event_ptr->attr.msg_buf_size = filler_size;
      filler_event_ptr->attr.msg_size = filler_size;
      filler_event_ptr->msg = std::string_view(kZeros, filler_size);

      // We've created the filler event, so adjust the original event accordingly.
      DCHECK(filler_size <= attr.bytes_missed);
      attr.msg_size = attr.msg_buf_size;
    }

    return filler_event_ptr;
  }

  std::string ToString() const {
    return absl::Substitute("attr:[$0] msg_size:$1 msg:[$2]", ::ToString(attr), msg.size(),
                            BytesToString<bytes_format::HexAsciiMix>(msg));
  }

  socket_data_event_t::attr_t attr;
  std::string_view msg;
};

}  // namespace stirling
}  // namespace px

// This template is in global namespace to allow absl to discover it.
template <typename H>
H AbslHashValue(H h, const struct conn_id_t& key) {
  return H::combine(std::move(h), key.upid.tgid, key.upid.start_time_ticks, key.fd, key.tsid);
}
