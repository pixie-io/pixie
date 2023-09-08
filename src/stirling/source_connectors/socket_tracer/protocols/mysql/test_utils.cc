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

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"

#include <math.h>
#include <deque>
#include <string>

#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {
namespace testutils {

std::string LengthEncodedInt(int num) {
  DCHECK(num < pow(2, 64));
  std::string s;
  if (num < 251) {
    char count_bytes[1];
    utils::IntToLEndianBytes(num, count_bytes);
    return std::string(CharArrayStringView(count_bytes));
  } else if (num < pow(2, 16)) {
    char count_bytes[2];
    utils::IntToLEndianBytes(num, count_bytes);
    return absl::StrCat("\xfc", CharArrayStringView(count_bytes));
  } else if (num < pow(2, 24)) {
    char count_bytes[3];
    utils::IntToLEndianBytes(num, count_bytes);
    return absl::StrCat("\xfd", CharArrayStringView(count_bytes));
  } else {
    char count_bytes[8];
    utils::IntToLEndianBytes(num, count_bytes);
    return absl::StrCat("\xfe", CharArrayStringView(count_bytes));
  }
}

template <size_t N>
std::string FixedLengthInt(int num) {
  char bytes[N];
  utils::IntToLEndianBytes(num, bytes);
  return std::string(bytes, N);
}

std::string LengthEncodedString(std::string_view s) {
  return absl::StrCat(LengthEncodedInt(s.size()), s);
}

/**
 * These Gen functions help generate raw string or Packet needed for testing the MySQL parser
 * or stitcher, respectively. The caller are expected to use structured events in test_data.h
 * and revert them back to strings or packets as test input.
 */

/**
 * Generates raw MySQL packets in the form of a string.
 */
std::string GenRawPacket(uint8_t packet_num, std::string_view msg) {
  char header[4];
  utils::IntToLEndianBytes(msg.size(), header);
  header[3] = packet_num;
  return absl::StrCat(CharArrayStringView(header), msg);
}

/**
 * Generate a raw MySQL packet from a Packet object.
 * @param packet The original Packet object.
 * @return the packet as a raw string, including header.
 */
std::string GenRawPacket(const Packet& packet) {
  return GenRawPacket(packet.sequence_id, packet.msg);
}

/**
 * Generates a raw packet with a string request.
 */
std::string GenRequestPacket(Command command, std::string_view msg) {
  return GenRawPacket(0, absl::StrCat(CommandToString(command), msg));
}

/**
 * Generates the header packet of Resultset response. It contains num of cols.
 */
Packet GenCountPacket(uint8_t seq_id, int num_col) {
  Packet p;
  p.sequence_id = seq_id;
  p.msg = LengthEncodedInt(num_col);
  return p;
}

/**
 * Generates a Col Definition packet. Can be used in StmtPrepareResponse or Resultset.
 */
Packet GenColDefinition(uint8_t seq_id, const ColDefinition& col_def) {
  Packet p;
  p.sequence_id = seq_id;
  p.msg = absl::StrCat(
      LengthEncodedString(col_def.catalog), LengthEncodedString(col_def.schema),
      LengthEncodedString(col_def.table), LengthEncodedString(col_def.org_table),
      LengthEncodedString(col_def.name), LengthEncodedString(col_def.org_name),
      LengthEncodedInt(col_def.next_length), FixedLengthInt<2>(col_def.character_set),
      FixedLengthInt<4>(col_def.column_length),
      std::string(1, static_cast<char>(col_def.column_type)), FixedLengthInt<2>(col_def.flags),
      FixedLengthInt<1>(col_def.decimals), "\x00\x00");
  return p;
}

/**
 * Generates a resultset row.
 */
Packet GenResultsetRow(uint8_t seq_id, const ResultsetRow& row) {
  Packet p;
  p.sequence_id = seq_id;
  p.msg = row.msg;
  return p;
}

/**
 * Generates a header of StmtPrepare Response.
 */
Packet GenStmtPrepareRespHeader(uint8_t seq_id, const StmtPrepareRespHeader& header) {
  char statement_id[4];
  char num_columns[2];
  char num_params[2];
  char warning_count[2];
  utils::IntToLEndianBytes(header.stmt_id, statement_id);
  utils::IntToLEndianBytes(header.num_columns, num_columns);
  utils::IntToLEndianBytes(header.num_params, num_params);
  utils::IntToLEndianBytes(header.warning_count, warning_count);
  std::string msg = absl::StrCat(ConstStringView("\x00"), CharArrayStringView(statement_id),
                                 CharArrayStringView(num_columns), CharArrayStringView(num_params),
                                 ConstStringView("\x00"), CharArrayStringView(warning_count));

  Packet p;
  p.sequence_id = seq_id;
  p.msg = std::move(msg);
  return p;
}

/**
 * Generates a deque of packets. Contains a col counter packet and n resultset rows.
 */
std::deque<Packet> GenResultset(const Resultset& resultset, bool client_eof_deprecate) {
  uint8_t seq_id = 1;

  std::deque<Packet> result;
  result.emplace_back(GenCountPacket(seq_id++, resultset.num_col));
  for (const ColDefinition& col_def : resultset.col_defs) {
    result.emplace_back(GenColDefinition(seq_id++, col_def));
  }
  if (!client_eof_deprecate) {
    result.emplace_back(GenEOF(seq_id++));
  }
  for (const ResultsetRow& row : resultset.results) {
    result.emplace_back(GenResultsetRow(seq_id++, row));
  }
  if (client_eof_deprecate) {
    result.emplace_back(GenOK(seq_id++));
  } else {
    result.emplace_back(GenEOF(seq_id++));
  }
  return result;
}

/**
 * Generates a StmtPrepareOkResponse.
 */
std::deque<Packet> GenStmtPrepareOKResponse(const StmtPrepareOKResponse& resp) {
  uint8_t seq_id = 1;

  std::deque<Packet> result;
  result.push_back(GenStmtPrepareRespHeader(seq_id++, resp.header));

  for (const ColDefinition& param_def : resp.param_defs) {
    result.push_back(GenColDefinition(seq_id++, param_def));
  }
  result.push_back(GenEOF(seq_id++));

  for (const ColDefinition& col_def : resp.col_defs) {
    result.push_back(GenColDefinition(seq_id++, col_def));
  }
  result.push_back(GenEOF(seq_id++));
  return result;
}

Packet GenStmtExecuteRequest(const StmtExecuteRequest& req) {
  char statement_id[4];
  utils::IntToLEndianBytes(req.stmt_id, statement_id);
  std::string msg =
      absl::StrCat(CommandToString(Command::kStmtExecute), CharArrayStringView(statement_id),
                   ConstStringView("\x00\x01\x00\x00\x00"));
  int num_params = req.params.size();
  if (num_params > 0) {
    for (int i = 0; i < (num_params + 7) / 8; i++) {
      msg += ConstStringView("\x00");
    }
    msg += "\x01";
  }
  for (const StmtExecuteParam& param : req.params) {
    switch (param.type) {
      // TODO(chengruizhe): Add more types.
      case ColType::kString:
        msg += ConstStringView("\xfe\x00");
        break;
      default:
        msg += ConstStringView("\xfe\x00");
        break;
    }
  }
  for (const StmtExecuteParam& param : req.params) {
    msg += LengthEncodedInt(param.value.size());
    msg += param.value;
  }
  Packet p;
  p.msg = std::move(msg);
  return p;
}

Packet GenStmtCloseRequest(const StmtCloseRequest& req) {
  char statement_id[4];
  utils::IntToLEndianBytes(req.stmt_id, statement_id);
  std::string msg =
      absl::StrCat(CommandToString(Command::kStmtClose), CharArrayStringView(statement_id));
  Packet p;
  p.msg = std::move(msg);
  return p;
}

/**
 * Generates a String Request packet of the specified type.
 */
Packet GenStringRequest(const StringRequest& req, Command command) {
  DCHECK_LE(static_cast<uint8_t>(command), kMaxCommandValue);
  Packet p;
  p.msg = absl::StrCat(CommandToString(command), req.msg);
  return p;
}

/**
 * Generates a Err packet.
 */
Packet GenErr(uint8_t seq_id, const ErrResponse& err) {
  char error_code[2];
  utils::IntToLEndianBytes(err.error_code, error_code);
  std::string msg = absl::StrCat("\xff", CharArrayStringView(error_code),
                                 "\x23\x48\x59\x30\x30\x30", err.error_message);
  Packet p;
  p.sequence_id = seq_id;
  p.msg = std::move(msg);
  return p;
}

/**
 * Generates a OK packet. Content is fixed.
 */
Packet GenOK(uint8_t seq_id) {
  std::string msg = ConstString("\x00\x00\x00\x02\x00\x00\x00");
  Packet p;
  p.sequence_id = seq_id;
  p.msg = std::move(msg);
  return p;
}

/**
 * Generates a EOF packet. Content is fixed.
 */
Packet GenEOF(uint8_t seq_id) {
  std::string msg = ConstString("\xfe\x00\x00\x22\x00");
  Packet p;
  p.sequence_id = seq_id;
  p.msg = std::move(msg);
  return p;
}

}  // namespace testutils
}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
