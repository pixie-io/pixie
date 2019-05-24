#pragma once

#include <string>

#include "src/stirling/bcc_bpf/socket_trace.h"

namespace pl {
namespace stirling {

/**
 * @brief Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf/socket_trace.h.
 */
struct SocketConnection {
  uint64_t timestamp_ns = 0;
  uint32_t tgid = 0;
  uint32_t fd = -1;
  std::string src_addr = "-";
  int src_port = -1;
  std::string dst_addr = "-";
  int dst_port = -1;
};

}  // namespace stirling
}  // namespace pl
