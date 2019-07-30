#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_stitcher.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * https://dev.mysql.com/doc/internals/en/packet-EOF_Packet.html
 */
bool IsEOFPacket(const Packet& packet) {
  // '\xfe' + warnings[2] + status_flags[2](If CLIENT_PROTOCOL_41).
  if (packet.msg.size() != 1 && packet.msg.size() != 5) {
    return false;
  }
  return packet.msg[0] == kEOFPrefix;
}

/**
 * https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
 */
bool IsErrPacket(const Packet& packet) {
  // It's at least 3 bytes, '\xff' + error_code.
  if (packet.msg.size() < 3) {
    return false;
  }
  return packet.msg[0] == kErrPrefix;
}

/**
 * https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
 */
bool IsOKPacket(const Packet& packet) {
  // 7 bytes is the minimum size for an OK packet. Read doc linked above for details.
  if (packet.msg.size() < 7) {
    return false;
  }
  return packet.msg[0] == kOKPrefix;
}

// TODO(chengruizhe): Since currently we don't intercept/store user capability flags, we don't know
// if e.g. CLIENT_DEPRECATE_EOF is set. So we pop off an EOF packet if there is one, and do nothing
// if there isn't one. https://dev.mysql.com/doc/internals/en/capability-flags.html
void ProcessEOFPacket(std::deque<Packet>* resp_packets) {
  Packet eof_packet = resp_packets->front();
  if (IsEOFPacket(eof_packet)) {
    resp_packets->pop_front();
  }
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
