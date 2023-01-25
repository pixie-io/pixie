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

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/packet_utils.h"

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/parse_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

/**
 * https://dev.mysql.com/doc/internals/en/packet-EOF_Packet.html
 */
bool IsEOFPacket(const Packet& packet, bool protocol_41) {
  // '\xfe' + warnings[2] + status_flags[2](If CLIENT_PROTOCOL_41).
  size_t expected_size = protocol_41 ? 5 : 1;

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
  return packet.msg[0] == static_cast<char>(kRespHeaderErr) && (packet.msg.size() > 3);
}

/**
 * Assume CLIENT_PROTOCOL_41 is set.
 * https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
 */
bool IsOKPacket(const Packet& packet) {
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
  if (warnings > 1000) {
    LOG_FIRST_N(WARNING, 10) << "Large warnings count is a sign of misclassification of OK packet.";
  }

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

// https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-ProtocolText::ResultsetRow
Status ProcessTextResultsetRowPacket(const Packet& packet, size_t num_col) {
  constexpr char kResultsetRowNullPrefix = '\xfb';
  if ((packet.msg.length() == 1) && (packet.msg.front() == kResultsetRowNullPrefix)) {
    return Status::OK();
  }
  std::string result;
  size_t offset = 0;
  for (size_t i = 0; i < num_col; ++i) {
    PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &result));
  }

  // Shouldn't have anything after the length encoded string.
  if (offset < packet.msg.length()) {
    return error::Internal("Have extra bytes in text resultset row.");
  }
  return Status::OK();
}

Status ProcessBinaryResultsetRowPacket(const Packet& packet,
                                       VectorView<ColDefinition> column_defs) {
  constexpr int kBinaryResultsetRowHeaderOffset = 1;
  constexpr int kBinaryResultsetRowNullBitmapOffset = 2;
  constexpr int kBinaryResultsetRowNullBitmapByteFiller = 7;

  if (packet.msg.at(0) != '\x00') {
    return error::Internal("Binary resultset row header mismatch.");
  }
  size_t null_bitmap_len = (column_defs.size() + kBinaryResultsetRowNullBitmapByteFiller +
                            kBinaryResultsetRowNullBitmapOffset) /
                           8;
  size_t offset = kBinaryResultsetRowHeaderOffset + null_bitmap_len;

  if (offset >= packet.msg.size()) {
    return error::Internal("Not enough bytes.");
  }

  std::string_view null_bitmap =
      std::string_view(packet.msg).substr(kBinaryResultsetRowHeaderOffset, null_bitmap_len);

  // Starting from the 3rd bit in the null_bitmap, each bit at pos i records if the the i-2th value
  // is null.
  for (size_t i = 0; i < column_defs.size(); ++i) {
    unsigned int null_bitmap_bytepos = (i + kBinaryResultsetRowNullBitmapOffset) / 8;
    unsigned int null_bitmap_bitpos = (i + kBinaryResultsetRowNullBitmapOffset) % 8;

    unsigned int is_null = (null_bitmap[null_bitmap_bytepos] >> null_bitmap_bitpos) & 1;

    if (is_null == 1) {
      continue;
    }

    std::string val;
    switch (column_defs[i].column_type) {
      case ColType::kString:
      case ColType::kVarChar:
      case ColType::kVarString:
      case ColType::kEnum:
      case ColType::kSet:
      case ColType::kLongBlob:
      case ColType::kMediumBlob:
      case ColType::kBlob:
      case ColType::kTinyBlob:
      case ColType::kGeometry:
      case ColType::kBit:
      case ColType::kDecimal:
      case ColType::kNewDecimal:
        PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &val));
        break;
      case ColType::kLongLong:
        PX_RETURN_IF_ERROR(DissectIntParam<8>(packet.msg, &offset, &val));
        break;
      case ColType::kLong:
      case ColType::kInt24:
        PX_RETURN_IF_ERROR(DissectIntParam<4>(packet.msg, &offset, &val));
        break;
      case ColType::kShort:
      case ColType::kYear:
        PX_RETURN_IF_ERROR(DissectIntParam<2>(packet.msg, &offset, &val));
        break;
      case ColType::kTiny:
        PX_RETURN_IF_ERROR(DissectIntParam<1>(packet.msg, &offset, &val));
        break;
      case ColType::kDouble:
        PX_RETURN_IF_ERROR(DissectFloatParam<double>(packet.msg, &offset, &val));
        break;
      case ColType::kFloat:
        PX_RETURN_IF_ERROR(DissectFloatParam<float>(packet.msg, &offset, &val));
        break;
      case ColType::kDate:
      case ColType::kDateTime:
      case ColType::kTimestamp:
      case ColType::kTime:
        // TODO(chengruizhe): Implement DissectDateTime correctly.
        PX_RETURN_IF_ERROR(DissectDateTimeParam(packet.msg, &offset, &val));
        break;
      default:
        return error::Internal("Unrecognized result column type.");
    }
  }

  if (offset != packet.msg.size()) {
    return error::Internal("Extra bytes in binary resultset row.");
  }
  return Status::OK();
}

bool IsStmtPrepareOKPacket(const Packet& packet) {
  // https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html#packet-COM_STMT_PREPARE_OK
  return (packet.msg.size() == 12U && packet.msg[0] == 0 && packet.msg[9] == 0);
}

StatusOr<ColDefinition> ProcessColumnDefPacket(const Packet& packet) {
  ColDefinition col_def;
  size_t offset = 0;
  PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &col_def.catalog));
  if (col_def.catalog.compare("def") != 0) {
    return error::Internal("ColumnDef Packet must start with `def`.");
  }

  PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &col_def.schema));
  PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &col_def.table));
  PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &col_def.org_table));
  PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &col_def.name));
  PX_RETURN_IF_ERROR(DissectStringParam(packet.msg, &offset, &col_def.org_name));
  PX_ASSIGN_OR_RETURN(col_def.next_length, ProcessLengthEncodedInt(packet.msg, &offset));
  if (col_def.next_length != 12) {
    return error::Internal("ColumnDef Packet's next_length field is always 0x0c.");
  }

  PX_RETURN_IF_ERROR(DissectInt<2>(packet.msg, &offset, &col_def.character_set));
  PX_RETURN_IF_ERROR(DissectInt<4>(packet.msg, &offset, &col_def.column_length));
  int8_t type;
  PX_RETURN_IF_ERROR(DissectInt<1>(packet.msg, &offset, &type));
  col_def.column_type = static_cast<ColType>(type);

  PX_RETURN_IF_ERROR(DissectInt<2>(packet.msg, &offset, &col_def.flags));
  PX_RETURN_IF_ERROR(DissectInt<1>(packet.msg, &offset, &col_def.decimals));

  return col_def;
}

// Look for SERVER_MORE_RESULTS_EXIST in Status field OK or EOF packet.
// Multi-resultsets only exist in protocol 4.1 and above.
bool MoreResultsExist(const Packet& last_packet) {
  constexpr uint8_t kServerMoreResultsExistFlag = 0x8;

  if (IsOKPacket(last_packet)) {
    size_t pos = 1;

    StatusOr<int> s1 = ProcessLengthEncodedInt(last_packet.msg, &pos);
    StatusOr<int> s2 = ProcessLengthEncodedInt(last_packet.msg, &pos);
    if (!s1.ok() || !s2.ok()) {
      LOG(ERROR) << "Error parsing OK packet for SERVER_MORE_RESULTS_EXIST_FLAG";
      return false;
    }

    return last_packet.msg[pos] & kServerMoreResultsExistFlag;
  }

  if (IsEOFPacket(last_packet, /* protocol_41 */ true)) {
    constexpr int kEOFPacketStatusPos = 3;
    return (last_packet.msg[kEOFPacketStatusPos] & kServerMoreResultsExistFlag);
  }

  return false;
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
