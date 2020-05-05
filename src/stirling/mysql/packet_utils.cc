#include "src/stirling/mysql/packet_utils.h"

#include "src/stirling/mysql/parse_utils.h"

namespace pl {
namespace stirling {
namespace mysql {

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
 * Assume CLIENT_PROTOCOL_41 is set.
 * https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
 */
bool IsOKPacket(const Packet& packet) {
  // TODO(oazizi): Remove static_cast once msg is converted to basic_string<uint8_t>.
  constexpr uint8_t kOKPacketHeaderOffset = 1;
  uint8_t header = packet.msg[0];

  // Parse affected_rows.
  size_t offset = kOKPacketHeaderOffset;
  if (!ProcessLengthEncodedInt(packet.msg, &offset).ok()) {
    return false;
  }
  // Parse last_insert_id.
  if (!ProcessLengthEncodedInt(packet.msg, &offset).ok()) {
    return false;
  }

  // Parse status flag.
  int16_t status_flag;
  if (!DissectInt<2, int16_t>(packet.msg, &offset, &status_flag).ok()) {
    return false;
  }

  // Parse warnings.
  int16_t warnings;
  if (!DissectInt<2, int16_t>(packet.msg, &offset, &warnings).ok()) {
    return false;
  }
  LOG_IF(WARNING, warnings > 1000)
      << "Large warnings count is a sign of misclassification of OK packet.";

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

  if (IsOKPacket(last_packet)) {
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
