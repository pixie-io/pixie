#include "src/stirling/mysql/mysql.h"

#include "src/common/base/byte_utils.h"
#include "src/stirling/mysql/mysql_stitcher.h"

namespace pl {
namespace stirling {
namespace mysql {

// TODO(oazizi): Move out to parse_utils.cc.
StatusOr<int64_t> ProcessLengthEncodedInt(std::string_view s, size_t* offset) {
  // If it is < 0xfb, treat it as a 1-byte integer.
  // If it is 0xfc, it is followed by a 2-byte integer.
  // If it is 0xfd, it is followed by a 3-byte integer.
  // If it is 0xfe, it is followed by a 8-byte integer.

  constexpr uint8_t kLencIntPrefix2b = 0xfc;
  constexpr uint8_t kLencIntPrefix3b = 0xfd;
  constexpr uint8_t kLencIntPrefix8b = 0xfe;

  if (*offset >= s.size()) {
    return error::Internal("Not enough bytes to extract length-encoded int");
  }

  int64_t result;
  int len;
  switch (static_cast<uint8_t>(s[*offset])) {
    case kLencIntPrefix2b:
      len = 2;
      ++*offset;
      break;
    case kLencIntPrefix3b:
      len = 3;
      ++*offset;
      break;
    case kLencIntPrefix8b:
      len = 8;
      ++*offset;
      break;
    default:
      len = 1;
      break;
  }

  if (*offset + len > s.size()) {
    return error::Internal("Not enough bytes to extract length-encoded int");
  }

  result = utils::LittleEndianByteStrToInt<uint64_t>(s.substr(*offset, len));
  *offset += len;

  return result;
}

/**
 * https://dev.mysql.com/doc/internals/en/packet-EOF_Packet.html
 */
bool IsEOFPacket(const Packet& packet, bool protocol_41) {
  // '\xfe' + warnings[2] + status_flags[2](If CLIENT_PROTOCOL_41).
  size_t expected_size = protocol_41 ? 5 : 1;

  // TODO(oazizi): Remove static_cast once msg is converted to basic_string<uint8_t>.
  uint8_t header = packet.msg[0];
  return ((header == kRespHeaderEOF) && (packet.msg.size() == expected_size));
}

bool IsEOFPacket(const Packet& packet) {
  return IsEOFPacket(packet, true) || IsEOFPacket(packet, false);
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
bool IsOKPacket(const Packet& packet, bool protocol_41) {
  // TODO(oazizi): Remove static_cast once msg is converted to basic_string<uint8_t>.

  uint8_t header = packet.msg[0];

  if (!protocol_41) {
    // 3 byte minimum packet size prior to protocol 4.1.
    return ((header == kRespHeaderOK) && (packet.msg.size() >= 3));
  }

  // Protocol 4.1 is more complicated.

  // 7 byte minimum packet size in protocol 4.1.
  if ((header == kRespHeaderOK) && (packet.msg.size() >= 7)) {
    return true;
  }

  // Some servers appear to still use the EOF marker in the OK response, even with
  // CLIENT_DEPRECATE_EOF.
  if ((header == kRespHeaderEOF) && (packet.msg.size() < 9) && !IsEOFPacket(packet)) {
    return true;
  }

  return false;
}

bool IsOKPacket(const Packet& packet) {
  return IsOKPacket(packet, true) || IsOKPacket(packet, false);
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

// Look for SERVER_MORE_RESULTS_EXIST in Status field OK or EOF packet.
// Multi-resultsets only exist in protocol 4.1 and above.
bool MoreResultsExists(const Packet& last_packet) {
  constexpr uint8_t kServerMoreResultsExistsFlag = 0x8;

  if (IsOKPacket(last_packet, /* protocol_41 */ true)) {
    size_t pos = 1;

    StatusOr<int> s1 = ProcessLengthEncodedInt(last_packet.msg, &pos);
    StatusOr<int> s2 = ProcessLengthEncodedInt(last_packet.msg, &pos);
    if (!s1.ok() || !s2.ok()) {
      LOG(ERROR) << "Error parsing OK packet for SERVER_MORE_RESULTS_EXIST_FLAG";
      return false;
    }

    return last_packet.msg[pos] & kServerMoreResultsExistsFlag;
  }

  if (IsEOFPacket(last_packet, /* protocol_41 */ true)) {
    constexpr int kEOFPacketStatusPos = 3;
    return (last_packet.msg[kEOFPacketStatusPos] & kServerMoreResultsExistsFlag);
  }

  return false;
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
