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

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/handler.h"

#include <string>
#include <utility>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/packet_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/parse_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

// Rules for return values:
//  error - The packet is invalid (mal-formed body) and cannot be parsed.
//  kNeedsMoreData - Additional packets are required.
//  kSuccess - Everything looks good.

#define RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets) \
  if (resp_packets.empty()) {                         \
    entry->resp.status = RespStatus::kUnknown;        \
    return ParseState::kNeedsMoreData;                \
  }

StatusOr<ParseState> HandleNoResponse(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      Record* entry) {
  if (!resp_packets.empty()) {
    return error::Internal("Did not expect any response packets [num_extra_packets=$0].",
                           resp_packets.size());
  }

  entry->resp.status = RespStatus::kNone;
  entry->resp.timestamp_ns = req_packet.timestamp_ns;

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleErrMessage(DequeView<Packet> resp_packets, Record* entry) {
  CTX_DCHECK(!resp_packets.empty());
  const Packet& packet = resp_packets.front();

  // Format of ERR packet:
  //   1  header: 0xff
  //   2  error_code
  //   1  sql_state_marker
  //   5  sql_state
  //   x  error_message
  // https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
  constexpr int kMinErrPacketSize = 9;
  constexpr int kErrorCodePos = 1;
  constexpr int kErrorCodeSize = 2;
  constexpr int kErrorMessagePos = 9;

  if (packet.msg.size() < kMinErrPacketSize) {
    return error::Internal("Insufficient number of bytes for an error packet.");
  }

  entry->resp.msg = packet.msg.substr(kErrorMessagePos);

  int error_code = utils::LEndianBytesToInt<int, kErrorCodeSize>(packet.msg.substr(kErrorCodePos));
  // TODO(oazizi): Add error code into resp msg.
  PX_UNUSED(error_code);

  entry->resp.status = RespStatus::kErr;
  entry->resp.timestamp_ns = packet.timestamp_ns;

  if (resp_packets.size() > 1) {
    return error::Internal(
        "Did not expect additional packets after error packet [num_extra_packets=$0].",
        resp_packets.size() - 1);
  }

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleOKMessage(DequeView<Packet> resp_packets, Record* entry) {
  CTX_DCHECK(!resp_packets.empty());
  const Packet& packet = resp_packets.front();

  // Format of OK packet:
  // https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
  constexpr int kMinOKPacketSize = 7;

  if (packet.msg.size() < kMinOKPacketSize) {
    return error::Internal("Insufficient number of bytes for an OK packet.");
  }

  entry->resp.status = RespStatus::kOK;
  entry->resp.timestamp_ns = packet.timestamp_ns;

  if (resp_packets.size() > 1) {
    return error::Internal(
        "Did not expect additional packets after OK packet [num_extra_packets=$0].",
        resp_packets.size() - 1);
  }

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleResultsetResponse(DequeView<Packet> resp_packets, Record* entry,
                                             bool binary_resultset, bool multi_resultset) {
  VLOG(3) << absl::Substitute("HandleResultsetResponse with $0 packets", resp_packets.size());

  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  const Packet& first_resp_packet = resp_packets.front();
  resp_packets.pop_front();

  // The last resultset of a multi-resultset is just an OK packet.
  if (multi_resultset && IsOKPacket(first_resp_packet)) {
    entry->resp.status = RespStatus::kOK;
    entry->resp.timestamp_ns = first_resp_packet.timestamp_ns;
    LOG_IF(ERROR, resp_packets.size() != 1)
        << absl::Substitute("Found $0 extra packets", resp_packets.size() - 1);
    return ParseState::kSuccess;
  }

  // Process header packet.
  size_t param_offset = 0;
  auto s = ProcessLengthEncodedInt(first_resp_packet.msg, &param_offset);
  if (!s.ok()) {
    entry->resp.status = RespStatus::kUnknown;
    return error::Internal("Unable to process header packet of resultset response.");
  }
  int num_col = s.ValueOrDie();

  if (param_offset != first_resp_packet.msg.size()) {
    entry->resp.status = RespStatus::kUnknown;
    return error::Internal("Extra bytes in length-encoded int packet.");
  }

  if (num_col == 0) {
    entry->resp.status = RespStatus::kUnknown;
    return error::Internal("HandleResultsetResponse(): num columns should never be 0.");
  }

  VLOG(3) << absl::Substitute("num_columns=$0", num_col);

  // A resultset has:
  //  1             column_count packet (*already accounted for*)
  //  column_count  column definition packets
  //  0 or 1        EOF packet (if CLIENT_DEPRECATE_EOF is false)
  //  0+            ResultsetRow packets (Spec says 1+, but have seen 0 in practice).
  //  1             OK or EOF packet
  // Must have at least the minimum number of remaining packets in a response.
  if (resp_packets.size() < static_cast<size_t>(num_col + 1)) {
    entry->resp.status = RespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  std::vector<ColDefinition> col_defs;
  for (int i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
    const Packet& packet = resp_packets.front();
    resp_packets.pop_front();

    auto s = ProcessColumnDefPacket(packet);
    if (!s.ok()) {
      entry->resp.status = RespStatus::kUnknown;
      return error::Internal("Expected column definition packet");
    }

    ColDefinition col_def = s.ValueOrDie();
    col_defs.push_back(std::move(col_def));
  }

  // Optional EOF packet.
  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  if (IsEOFPacket(resp_packets.front())) {
    resp_packets.pop_front();
  }

  std::vector<ResultsetRow> results;

  auto isLastPacket = [](const Packet& p) {
    return (IsErrPacket(p) || IsOKPacket(p) || IsEOFPacket(p));
  };

  while (!resp_packets.empty()) {
    const Packet& row_packet = resp_packets.front();

    Status s;
    // TODO(chengruizhe): Get actual results from the resultset row packets if needed.
    // Attempt to process it as a resultset row packet first. Process[Text/Binary]ResultRowPacket
    // functions, if returning ok, indicates with very high confidence that the packet is indeed a
    // resultset row packet. IsOKPacket, on the other hand, is not as robust.
    if (binary_resultset) {
      s = ProcessBinaryResultsetRowPacket(row_packet, col_defs);
    } else {
      s = ProcessTextResultsetRowPacket(row_packet, col_defs.size());
    }

    if (s.ok()) {
      resp_packets.pop_front();
      ResultsetRow row{row_packet.msg};
      results.emplace_back(std::move(row));
    } else if (isLastPacket(row_packet)) {
      break;
    } else {
      entry->resp.status = RespStatus::kUnknown;
      return error::Internal("Expected resultset row packet [OK=$0 ERR=$1 EOF=$2]",
                             IsOKPacket(row_packet), IsErrPacket(row_packet),
                             IsEOFPacket(row_packet));
    }
  }

  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  const Packet& last_packet = resp_packets.front();

  CTX_DCHECK(isLastPacket(resp_packets.front()));
  if (IsErrPacket(resp_packets.front())) {
    return HandleErrMessage(resp_packets, entry);
  }

  resp_packets.pop_front();

  if (multi_resultset) {
    absl::StrAppend(&entry->resp.msg, ", ");
  }
  absl::StrAppend(&entry->resp.msg, "Resultset rows = ", results.size());

  // Check for another resultset in case this is a multi-resultset.
  if (MoreResultsExist(last_packet)) {
    return HandleResultsetResponse(resp_packets, entry, binary_resultset,
                                   /* multiresultset */ true);
  }

  LOG_IF(ERROR, !resp_packets.empty())
      << absl::Substitute("Found $0 extra packets", resp_packets.size());

  entry->resp.status = RespStatus::kOK;
  entry->resp.timestamp_ns = last_packet.timestamp_ns;
  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleStmtPrepareOKResponse(DequeView<Packet> resp_packets, State* state,
                                                 Record* entry) {
  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  const Packet& first_resp_packet = resp_packets.front();
  resp_packets.pop_front();
  if (!IsStmtPrepareOKPacket(first_resp_packet)) {
    entry->resp.status = RespStatus::kUnknown;
    return error::Internal("Expected StmtPrepareOK packet");
  }

  int stmt_id = utils::LEndianBytesToInt<int, 4>(first_resp_packet.msg.substr(1));
  size_t num_col = utils::LEndianBytesToInt<size_t, 2>(first_resp_packet.msg.substr(5));
  size_t num_param = utils::LEndianBytesToInt<size_t, 2>(first_resp_packet.msg.substr(7));
  size_t warning_count = utils::LEndianBytesToInt<size_t, 2>(first_resp_packet.msg.substr(10));

  // TODO(chengruizhe): Handle missing packets more robustly. Assuming no missing packet.
  // If num_col or num_param is non-zero, they might be followed by EOF.
  // Reference: https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html.
  size_t min_expected_packets = num_col + num_param;
  if (min_expected_packets > resp_packets.size()) {
    entry->resp.status = RespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  StmtPrepareRespHeader resp_header{stmt_id, num_col, num_param, warning_count};
  entry->resp.timestamp_ns = first_resp_packet.timestamp_ns;

  // Params come before columns
  std::vector<ColDefinition> param_defs;
  for (size_t i = 0; i < num_param; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
    const Packet& param_def_packet = resp_packets.front();
    resp_packets.pop_front();

    auto s = ProcessColumnDefPacket(param_def_packet);
    if (!s.ok()) {
      entry->resp.status = RespStatus::kUnknown;
      return error::Internal("Fail to process param definition packet.");
    }

    param_defs.push_back(s.ConsumeValueOrDie());
    entry->resp.timestamp_ns = param_def_packet.timestamp_ns;
  }

  if (num_param != 0) {
    // Optional EOF packet, based on CLIENT_DEPRECATE_EOF. But difficult to infer
    // CLIENT_DEPRECATE_EOF because num_param can be zero.
    if (!resp_packets.empty()) {
      const Packet& eof_packet = resp_packets.front();
      if (IsEOFPacket(eof_packet)) {
        resp_packets.pop_front();
        entry->resp.timestamp_ns = eof_packet.timestamp_ns;
      }
    }
  }

  std::vector<ColDefinition> col_defs;
  for (size_t i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
    const Packet& col_def_packet = resp_packets.front();
    resp_packets.pop_front();

    auto s = ProcessColumnDefPacket(col_def_packet);
    if (!s.ok()) {
      entry->resp.status = RespStatus::kUnknown;
      return error::Internal("Fail to process column definition packet.");
    }

    col_defs.push_back(s.ConsumeValueOrDie());
    // Update timestamp, in case this turns out to be the last packet.
    entry->resp.timestamp_ns = col_def_packet.timestamp_ns;
  }

  // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
  if (num_col != 0) {
    // Optional EOF packet, based on CLIENT_DEPRECATE_EOF. But difficult to infer
    // CLIENT_DEPRECATE_EOF because num_param can be zero.
    if (!resp_packets.empty()) {
      const Packet& eof_packet = resp_packets.front();
      if (IsEOFPacket(eof_packet)) {
        resp_packets.pop_front();
        entry->resp.timestamp_ns = eof_packet.timestamp_ns;
      }
    }
  }

  if (!resp_packets.empty()) {
    LOG(ERROR) << "Extra packets";
  }

  // Update state.
  state->prepared_statements.emplace(
      stmt_id,
      PreparedStatement{.request = entry->req.msg,
                        .response = StmtPrepareOKResponse{.header = resp_header,
                                                          .col_defs = std::move(col_defs),
                                                          .param_defs = std::move(param_defs)}});

  entry->resp.status = RespStatus::kOK;
  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleStringRequest(const Packet& req_packet, Record* entry) {
  if (req_packet.msg.empty()) {
    return error::Internal("A request cannot have an empty payload.");
  }
  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.msg = req_packet.msg.substr(1);
  entry->req.timestamp_ns = req_packet.timestamp_ns;

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleNonStringRequest(const Packet& req_packet, Record* entry) {
  if (req_packet.msg.empty()) {
    return error::Internal("A request cannot have an empty payload.");
  }
  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.msg.clear();
  entry->req.timestamp_ns = req_packet.timestamp_ns;

  return ParseState::kSuccess;
}

namespace {
std::string CombinePrepareExecute(std::string_view stmt_prepare_request,
                                  const std::vector<StmtExecuteParam>& params) {
  std::string result = absl::Substitute("query=[$0] params=[", stmt_prepare_request);
  // Implements absl::StrJoin manually to avoid the extra copies going from
  // std::vector<StmtExecuteParam> to std::vector<std::string> and then std::string via
  // absl::StrJoin.
  for (const auto& [i, param] : Enumerate(params)) {
    result += param.value;
    if (i < params.size() - 1) {
      result += ", ";
    }
  }
  result += "]";
  return result;
}

// Spec on how to dissect params is here:
// https://dev.mysql.com/doc/internals/en/binary-protocol-value.html
//
// List of parameter types is followed by list of parameter values,
// so we have two offset pointers, one that points to current type position,
// and one that points to current value position
Status ProcessStmtExecuteParam(std::string_view msg, size_t* type_offset, size_t* val_offset,
                               StmtExecuteParam* param) {
  param->type = static_cast<ColType>(msg[*type_offset]);
  type_offset += 2;

  switch (param->type) {
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
      PX_RETURN_IF_ERROR(DissectStringParam(msg, val_offset, &param->value));
      break;
    case ColType::kTiny:
      PX_RETURN_IF_ERROR(DissectIntParam<1>(msg, val_offset, &param->value));
      break;
    case ColType::kShort:
    case ColType::kYear:
      PX_RETURN_IF_ERROR(DissectIntParam<2>(msg, val_offset, &param->value));
      break;
    case ColType::kLong:
    case ColType::kInt24:
      PX_RETURN_IF_ERROR(DissectIntParam<4>(msg, val_offset, &param->value));
      break;
    case ColType::kLongLong:
      PX_RETURN_IF_ERROR(DissectIntParam<8>(msg, val_offset, &param->value));
      break;
    case ColType::kFloat:
      PX_RETURN_IF_ERROR(DissectFloatParam<float>(msg, val_offset, &param->value));
      break;
    case ColType::kDouble:
      PX_RETURN_IF_ERROR(DissectFloatParam<double>(msg, val_offset, &param->value));
      break;
    case ColType::kDate:
    case ColType::kDateTime:
    case ColType::kTimestamp:
      PX_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, &param->value));
      break;
    case ColType::kTime:
      PX_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, &param->value));
      break;
    case ColType::kNull:
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unexpected/unhandled column type $0",
                                      static_cast<int>(param->type));
  }

  return Status::OK();
}

}  // namespace

StatusOr<ParseState> HandleStmtExecuteRequest(const Packet& req_packet,
                                              std::map<int, PreparedStatement>* prepare_map,
                                              Record* entry) {
  if (req_packet.msg.size() < 1 + kStmtIDBytes) {
    return error::Internal("Insufficient number of bytes for STMT_EXECUTE");
  }

  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.timestamp_ns = req_packet.timestamp_ns;

  int stmt_id =
      utils::LEndianBytesToInt<int, kStmtIDBytes>(req_packet.msg.substr(kStmtIDStartOffset));

  auto iter = prepare_map->find(stmt_id);
  if (iter == prepare_map->end()) {
    // There can be 2 possibilities in this case:
    // 1. The stitcher is confused/messed up and accidentally deleted wrong prepare event.
    // 2. Client sent a Stmt Exec for a deleted Stmt Prepare
    // We return -1 as stmt_id to indicate error and defer decision to the caller.

    // We can't determine whether the rest of this packet is valid or not, so just return success.
    // But pass the information up.
    entry->px_info = absl::Substitute(
        "Could not find PREPARE statement for this EXECUTE command. Query not decoded. "
        "[stmt_id=$0].",
        stmt_id);
    entry->req.msg = absl::Substitute("Execute stmt_id=$0.", stmt_id);
    return ParseState::kSuccess;
  }

  int num_params = iter->second.response.header.num_params;

  size_t offset = kStmtIDStartOffset + kStmtIDBytes + kFlagsBytes + kIterationCountBytes;

  if (req_packet.msg.size() < offset) {
    return error::Internal("Not a valid StmtExecuteRequest");
  }

  // This is copied directly from the MySQL spec.
  const int null_bitmap_length = (num_params + 7) / 8;
  offset += null_bitmap_length;
  uint8_t stmt_bound = req_packet.msg[offset];
  offset += 1;

  std::vector<StmtExecuteParam> params;
  if (stmt_bound == 1) {
    // Offset to first param type and first param value respectively.
    // Each call to ProcessStmtExecuteParam will advance the two offsets to their next positions.
    size_t param_type_offset = offset;
    size_t param_val_offset = offset + 2 * num_params;

    for (int i = 0; i < num_params; ++i) {
      StmtExecuteParam param;
      PX_RETURN_IF_ERROR(
          ProcessStmtExecuteParam(req_packet.msg, &param_type_offset, &param_val_offset, &param));
      params.emplace_back(param);
    }
  }

  std::string_view stmt_prepare_request = iter->second.request;
  entry->req.msg = CombinePrepareExecute(stmt_prepare_request, params);

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleStmtCloseRequest(const Packet& req_packet,
                                            std::map<int, PreparedStatement>* prepare_map,
                                            Record* entry) {
  if (req_packet.msg.size() < 1 + kStmtIDBytes) {
    return error::Internal("Insufficient number of bytes for STMT_CLOSE");
  }

  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.msg = "";
  entry->req.timestamp_ns = req_packet.timestamp_ns;

  int stmt_id =
      utils::LEndianBytesToInt<int, kStmtIDBytes>(req_packet.msg.substr(kStmtIDStartOffset));
  auto iter = prepare_map->find(stmt_id);
  if (iter != prepare_map->end()) {
    prepare_map->erase(iter);
  } else {
    // We may have missed the prepare statement (e.g. due to the missing start of connection
    // problem), but we can still process the close, and continue on. Just print a warning.
    entry->px_info = absl::Substitute(
        "Could not find prepare statement for this close command [stmt_id=$0].", stmt_id);
  }

  return ParseState::kSuccess;
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
