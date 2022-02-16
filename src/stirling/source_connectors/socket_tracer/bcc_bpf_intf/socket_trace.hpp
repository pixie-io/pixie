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
  return absl::Substitute(
      "[ts=$0 conn_id=$1 protocol=$2 role=$3 dir=$4 ssl=$5 source_fn=$6 pos=$7 size=$8 "
      "buf_size=$9]",
      attr.timestamp_ns, ToString(attr.conn_id), magic_enum::enum_name(attr.protocol),
      magic_enum::enum_name(attr.role), magic_enum::enum_name(attr.direction), attr.ssl,
      magic_enum::enum_name(attr.source_fn), attr.pos, attr.msg_size, attr.msg_buf_size);
}

inline std::string ToString(const close_event_t& event) {
  return absl::Substitute("[wr_bytes=$0 rd_bytes=$1]", event.wr_bytes, event.rd_bytes);
}

inline std::string ToString(const conn_event_t& event) {
  return absl::Substitute("[addr=$0]",
                          ::px::ToString(reinterpret_cast<const struct sockaddr*>(&event.addr)));
}

inline std::string ToString(const socket_control_event_t& event) {
  return absl::Substitute("[type=$0 ts=$1 conn_id=$2 $3]", magic_enum::enum_name(event.type),
                          event.timestamp_ns, ToString(event.conn_id),
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
    // Work around the memory alignment issue by using memcopy, instead of structure assignment.
    //
    // A known fact is that perf buffer's memory region is 8 bytes aligned. But each submission
    // has 2 parts, a 4 bytes size, and the following region with the specified size.
    //
    // When submitting memory to perf buffer, the memory region must be at least align on 4 bytes.
    // See http://man7.org/linux/man-pages/man2/perf_event_open.2.html (search for size, data[size],
    // which is what's used by BCC when open perf buffer.
    memcpy(&attr, static_cast<const char*>(data) + offsetof(socket_data_event_t, attr),
           sizeof(socket_data_event_t::attr_t));

    // The length header of the first Kafka packet on the server side will be dropped in bpf
    // due to protocol inference. We send the length header in attributes instead, and adjust pos
    // forward by 4 bytes.
    if (attr.prepend_length_header) {
      char buf[4];
      px::utils::IntToLEndianBytes(attr.length_header, buf);
      msg.assign(buf, 4);
      attr.pos -= 4;
    }
    // Use attr.msg_buf_size to only copy the data included in the buffer.
    // msg_buf_size may differ from msg_size when the message has been truncated or
    // when only metadata is being sent (e.g. unknown protocols or disabled trackers).
    msg.append(static_cast<const char*>(data) + offsetof(socket_data_event_t, msg),
               attr.msg_buf_size);

    // Hack: Create a filler event for sendfile data.
    // We need a better long-term solution for this,
    // since we aren't able to directly trace the data.
    if (attr.msg_buf_size != attr.msg_size) {
      DCHECK_GT(attr.msg_size, attr.msg_buf_size);
      VLOG(1) << "Adding filler to event";

      // Limit the size so we don't have huge allocations.
      constexpr uint32_t kMaxFilledSizeBytes = 1 * 1024 * 1024;

      size_t filled_size = std::min(attr.msg_size, kMaxFilledSizeBytes);
      msg.resize(filled_size, 0);
      if (attr.msg_size > kMaxFilledSizeBytes) {
        VLOG(1) << absl::Substitute("Event too large: $0", attr.msg_size);
      }
    }
  }

  std::string ToString() const {
    return absl::Substitute("attr:[$0] msg_size:$1 msg:[$2]", ::ToString(attr), msg.size(),
                            BytesToString<bytes_format::HexAsciiMix>(msg));
  }

  socket_data_event_t::attr_t attr;
  // TODO(oazizi/yzhao): Eventually, we will write the data into a buffer that can be used for later
  // parsing. By then, msg can be changed to string_view.
  std::string msg;
};

}  // namespace stirling
}  // namespace px

// This template is in global namespace to allow ABSL library to discover.
template <typename H>
H AbslHashValue(H h, const struct conn_id_t& key) {
  return H::combine(std::move(h), key.upid.tgid, key.upid.start_time_ticks, key.fd, key.tsid);
}
