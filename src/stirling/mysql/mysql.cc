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
  // TODO(oazizi): Remove static_cast once msg is converted to basic_string<uint8_t>.
  return (packet.msg[0] == static_cast<char>(kRespHeaderEOF)) &&
         ((packet.msg.size() == 1 || packet.msg.size() == 5));
}

/**
 * https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
 */
bool IsErrPacket(const Packet& packet) {
  // It's at least 3 bytes, '\xff' + error_code.
  // TODO(oazizi): Remove static_cast once msg is converted to basic_string<uint8_t>.
  return packet.msg[0] == static_cast<char>(kRespHeaderErr) && (packet.msg.size() > 3);
}

/**
 * https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
 */
bool IsOKPacket(const Packet& packet) {
  // TODO(oazizi): Remove static_cast once msg is converted to basic_string<uint8_t>.

  // 3 bytes is the minimum size for an OK packet. Read doc linked above for details.
  if (packet.msg[0] == static_cast<char>(kRespHeaderOK) && packet.msg.size() >= 3) {
    return true;
  }

  // Some servers appear to still use the EOF marker in the OK response, even with
  // CLIENT_DEPRECATE_EOF.
  if (packet.msg[0] == static_cast<char>(kRespHeaderEOF) && packet.msg.size() < 9) {
    if (IsEOFPacket(packet)) {
      return false;
    }
    return true;
  }

  return false;
}

bool IsLengthEncodedIntPacket(const Packet& packet) {
  constexpr uint8_t kLencIntPrefix2b = 0xfc;
  constexpr uint8_t kLencIntPrefix3b = 0xfd;
  constexpr uint8_t kLencIntPrefix8b = 0xfe;

  switch (static_cast<uint8_t>(packet.msg[0])) {
    case kLencIntPrefix8b:
      return packet.msg.size() == 9;
    case kLencIntPrefix3b:
      return packet.msg.size() == 4;
    case kLencIntPrefix2b:
      return packet.msg.size() == 3;
    default:
      return packet.msg.size() == 1 && (static_cast<uint8_t>(packet.msg[0]) < 251);
  }
}

bool IsColumnDefPacket(const Packet& packet) {
  // TODO(oazizi): This is a weak placeholder.
  // Study link below for a stronger implementation.
  // https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnDefinition

  return !(IsEOFPacket(packet) || IsOKPacket(packet) || IsErrPacket(packet));
}

bool IsResultsetRowPacket(const Packet& packet, bool client_deprecate_eof) {
  // TODO(oazizi): This is a weak placeholder.
  // Study link below for a stronger implementation.
  // https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-ProtocolText::ResultsetRow

  return (client_deprecate_eof ? !IsOKPacket(packet) : !IsEOFPacket(packet)) &&
         !IsErrPacket(packet);
}

bool IsStmtPrepareOKPacket(const Packet& packet) {
  // https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html#packet-COM_STMT_PREPARE_OK
  return (packet.msg.size() == 12U && packet.msg[0] == 0 && packet.msg[9] == 0);
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
