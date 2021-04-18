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

// This header defines the C++ counterparts of the BPF data structures.
// The file name is kept identical to its BPF counterpart as well.

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

    // Use attr.msg_buf_size to only copy the data included in the buffer.
    // msg_buf_size may differ from msg_size when the message has been truncated or
    // when only metadata is being sent (e.g. unknown protocols or disabled trackers).
    msg.assign(static_cast<const char*>(data) + offsetof(socket_data_event_t, msg),
               attr.msg_buf_size);
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

inline bool operator==(const struct conn_id_t& lhs, const struct conn_id_t& rhs) {
  return lhs.upid.tgid == rhs.upid.tgid && lhs.upid.start_time_ticks == rhs.upid.start_time_ticks &&
         lhs.fd == rhs.fd && lhs.tsid == rhs.tsid;
}

// This template is in global namespace to allow ABSL library to discover.
template <typename H>
H AbslHashValue(H h, const struct conn_id_t& key) {
  return H::combine(std::move(h), key.upid.tgid, key.upid.start_time_ticks, key.fd, key.tsid);
}
