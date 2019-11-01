#pragma once

#include <chrono>
#include <string>

#include "src/stirling/common/utils.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * Raw MySQLPacket from MySQL Parser
 */
struct Packet {
  TimeSpan time_span;
  uint64_t timestamp_ns = 0;
  std::chrono::time_point<std::chrono::steady_clock> creation_timestamp =
      std::chrono::steady_clock::now();

  uint8_t sequence_id = 0;
  // TODO(oazizi): Convert to std::basic_string<uint8_t>.
  std::string msg;

  size_t ByteSize() const { return sizeof(Packet) + msg.size(); }
};

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
