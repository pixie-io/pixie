#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

extern "C" {
#include "src/stirling/bcc_bpf/socket_trace.h"
}

// This header defines the C++ counterparts of the BPF data structures.
// The file name is kept identical to its BPF counterpart as well.

namespace pl {
namespace stirling {

/**
 * @brief A C++ friendly counterpart to socket_data_event_t. The memory buffer is managed through a
 * std::string, instead of the "struct hack" in C: http://c-faq.com/struct/structhack.html.
 *
 * Note that socket_data_event_t::msg cannot go beyond the fixed size, because of it's used inside
 * BPF. That's a minor difference to the ordinary struct hack.
 */
struct SocketDataEvent {
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
    msg.assign(static_cast<const char*>(data) + offsetof(socket_data_event_t, msg), attr.msg_size);
  }
  socket_data_event_t::attr_t attr;
  // TODO(oazizi/yzhao): Eventually, we will write the data into a buffer that can be used for later
  // parsing. By then, msg can be changed to string_view.
  std::string msg;
};

struct TimestampedData {
  explicit TimestampedData(std::unique_ptr<SocketDataEvent> event) {
    timestamp_ns = event->attr.timestamp_ns;
    msg = std::move(event->msg);
  }
  uint64_t timestamp_ns;
  std::string msg;
};

}  // namespace stirling
}  // namespace pl
