#include "src/stirling/mysql/mysql_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/stirling/mysql/packet_utils.h"
#include "src/stirling/mysql/parse_utils.h"
#include "src/stirling/mysql/types.h"

namespace pl {
namespace stirling {
namespace mysql {

// Rules for return values:
//  error - The packet is invalid (mal-formed body) and cannot be parsed.
//  kNeedsMoreData - Additional packets are required.
//  kSuccess - Everything looks good.

#define RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets) \
  if (iter == resp_packets.end()) {                       \
    entry->resp.status = MySQLRespStatus::kUnknown;       \
    return ParseState::kNeedsMoreData;                    \
  }

StatusOr<ParseState> HandleErrMessage(DequeView<Packet> resp_packets, Record* entry) {
  DCHECK(!resp_packets.empty());
  const Packet& packet = resp_packets.front();

  // TODO(chengruizhe): Assuming CLIENT_PROTOCOL_41 here. Make it more robust.
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
  PL_UNUSED(error_code);

  entry->resp.status = MySQLRespStatus::kErr;
  entry->resp.timestamp_ns = packet.timestamp_ns;

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleOKMessage(DequeView<Packet> resp_packets, Record* entry) {
  DCHECK(!resp_packets.empty());
  const Packet& packet = resp_packets.front();

  // Format of OK packet:
  // https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
  constexpr int kMinOKPacketSize = 7;

  if (packet.msg.size() < kMinOKPacketSize) {
    return error::Internal("Insufficient number of bytes for an OK packet.");
  }

  entry->resp.status = MySQLRespStatus::kOK;
  entry->resp.timestamp_ns = resp_packets.front().timestamp_ns;

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleResultsetResponse(DequeView<Packet> resp_packets, Record* entry,
                                             bool multi_resultset) {
  auto iter = resp_packets.begin();

  VLOG(3) << absl::Substitute("HandleResultsetResponse with $0 packets", resp_packets.size());

  RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);
  const Packet& first_resp_packet = *iter;

  // The last resultset of a multi-resultset is just an OK packet.
  if (multi_resultset && IsOKPacket(first_resp_packet)) {
    entry->resp.status = MySQLRespStatus::kOK;
    entry->resp.timestamp_ns = first_resp_packet.timestamp_ns;
    LOG_IF(ERROR, resp_packets.size() != 1)
        << absl::Substitute("Found $0 extra packets", resp_packets.size() - 1);
    return ParseState::kSuccess;
  }

  // Process header packet.
  if (!IsLengthEncodedIntPacket(first_resp_packet)) {
    entry->resp.status = MySQLRespStatus::kUnknown;
    return error::Internal("First packet should be length-encoded integer.");
  }

  size_t param_offset = 0;
  PL_ASSIGN_OR_RETURN(int num_col, ProcessLengthEncodedInt(first_resp_packet.msg, &param_offset));
  if (num_col == 0) {
    return error::Internal("HandleResultsetResponse(): num columns should never be 0.");
  }

  VLOG(3) << absl::Substitute("num_columns=$0", num_col);

  // A resultset has:
  //  1             column_count packet
  //  column_count  column definition packets
  //  0 or 1        EOF packet (if CLIENT_DEPRECATE_EOF is false)
  //  1+            ResultsetRow packets
  //  1             OK or EOF packet
  // Must have at least the minimum number of packets in a response.
  if (resp_packets.size() < static_cast<size_t>(3 + num_col)) {
    entry->resp.status = MySQLRespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  // Go to next packet.
  ++iter;

  std::vector<ColDefinition> col_defs;
  for (int i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);

    if (!IsColumnDefPacket(*iter)) {
      entry->resp.status = MySQLRespStatus::kUnknown;
      return error::Internal("Expected column definition packet");
    }

    const Packet& col_def_packet = *iter;
    ColDefinition col_def{col_def_packet.msg};
    col_defs.push_back(std::move(col_def));
    ++iter;
  }

  // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
  bool client_deprecate_eof = true;
  RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);
  if (IsEOFPacket(*iter)) {
    client_deprecate_eof = false;
    ++iter;
  }

  std::vector<ResultsetRow> results;

  auto isLastPacket = [client_deprecate_eof](const Packet& p) {
    // Depending on CLIENT_DEPRECATE_EOF, we may either get an OK or EOF packet.
    return IsErrPacket(p) || (client_deprecate_eof ? IsOKPacket(p) : IsEOFPacket(p));
  };

  while (iter != resp_packets.end() && !isLastPacket(*iter)) {
    const Packet& row_packet = *iter;
    if (!IsResultsetRowPacket(row_packet, client_deprecate_eof)) {
      entry->resp.status = MySQLRespStatus::kUnknown;
      return error::Internal(
          "Expected resultset row packet [OK=$0 ERR=$1 EOF=$2 client_deprecate_eof=$3]",
          IsOKPacket(row_packet), IsErrPacket(row_packet), IsEOFPacket(row_packet),
          client_deprecate_eof);
    }
    ResultsetRow row{row_packet.msg};
    results.emplace_back(std::move(row));
    ++iter;
  }

  RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);

  const Packet& last_packet = *iter;
  DCHECK(isLastPacket(last_packet));
  if (IsErrPacket(last_packet)) {
    // TODO(chengruizhe/oazizi): If it ends with err packet, propagate up error_message.
    PL_UNUSED(last_packet);
  }

  if (multi_resultset) {
    absl::StrAppend(&entry->resp.msg, ", ");
  }
  absl::StrAppend(&entry->resp.msg, "Resultset rows = ", results.size());

  // Check for another resultset in case this is a multi-resultset.
  if (MoreResultsExists(last_packet)) {
    DequeView<Packet> remaining_packets(resp_packets);
    remaining_packets.pop_front(iter - resp_packets.begin() + 1);
    return HandleResultsetResponse(remaining_packets, entry, true);
  }

  ++iter;
  LOG_IF(ERROR, iter != resp_packets.end())
      << absl::Substitute("Found $0 extra packets", std::distance(iter, resp_packets.end()));

  entry->resp.status = MySQLRespStatus::kOK;
  entry->resp.timestamp_ns = last_packet.timestamp_ns;
  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleStmtPrepareOKResponse(DequeView<Packet> resp_packets, State* state,
                                                 Record* entry) {
  auto iter = resp_packets.begin();

  RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);
  const Packet& first_resp_packet = *iter;
  if (!IsStmtPrepareOKPacket(first_resp_packet)) {
    entry->resp.status = MySQLRespStatus::kUnknown;
    return error::Internal("Expected StmtPrepareOK packet");
  }

  int stmt_id = utils::LEndianBytesToInt<int, 4>(first_resp_packet.msg.substr(1));
  size_t num_col = utils::LEndianBytesToInt<size_t, 2>(first_resp_packet.msg.substr(5));
  size_t num_param = utils::LEndianBytesToInt<size_t, 2>(first_resp_packet.msg.substr(7));
  size_t warning_count = utils::LEndianBytesToInt<size_t, 2>(first_resp_packet.msg.substr(10));

  // TODO(chengruizhe): Handle missing packets more robustly. Assuming no missing packet.
  // If num_col or num_param is non-zero, they will be followed by EOF.
  // Reference: https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html.
  size_t expected_num_packets = 1 + num_col + num_param + (num_col != 0) + (num_param != 0);
  if (expected_num_packets > resp_packets.size()) {
    entry->resp.status = MySQLRespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  StmtPrepareRespHeader resp_header{stmt_id, num_col, num_param, warning_count};

  ++iter;

  // Params come before columns
  std::vector<ColDefinition> param_defs;
  for (size_t i = 0; i < num_param; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);

    const Packet& param_def_packet = *iter;
    ColDefinition param_def{param_def_packet.msg};
    param_defs.push_back(std::move(param_def));
    ++iter;
  }

  bool client_deprecate_eof = true;
  if (num_param != 0) {
    RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);
    // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
    if (IsEOFPacket(*iter)) {
      ++iter;
      client_deprecate_eof = false;
    }
  }

  std::vector<ColDefinition> col_defs;
  for (size_t i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets);

    const Packet& col_def_packet = *iter;
    ColDefinition col_def{col_def_packet.msg};
    col_defs.push_back(std::move(col_def));
    ++iter;
  }

  // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
  if (!client_deprecate_eof && num_col != 0) {
    if (iter == resp_packets.end()) {
      LOG(ERROR) << "Ignoring EOF packet that is expected, but appears to be missing.";
    } else if (IsEOFPacket(*iter)) {
      ++iter;
    }
  }

  if (iter != resp_packets.end()) {
    LOG(ERROR) << "Extra packets";
  }

  // Update state.
  state->prepared_statements.emplace(
      stmt_id,
      PreparedStatement{.request = entry->req.msg,
                        .response = StmtPrepareOKResponse{.header = resp_header,
                                                          .col_defs = std::move(col_defs),
                                                          .param_defs = std::move(param_defs)}});

  // Go back one to get last packet.
  const Packet& last_packet = *(--iter);

  entry->resp.status = MySQLRespStatus::kOK;
  entry->resp.timestamp_ns = last_packet.timestamp_ns;
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
  size_t offset = 0;
  size_t count = 0;
  std::string result;

  for (size_t index = stmt_prepare_request.find("?", offset); index != std::string::npos;
       index = stmt_prepare_request.find("?", offset)) {
    if (count >= params.size()) {
      LOG(WARNING) << "Unequal number of stmt exec parameters for stmt prepare.";
      break;
    }
    absl::StrAppend(&result, stmt_prepare_request.substr(offset, index - offset),
                    params[count].value);
    count++;
    offset = index + 1;
  }
  result += stmt_prepare_request.substr(offset);

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
  param->type = static_cast<MySQLColType>(msg[*type_offset]);
  type_offset += 2;

  switch (param->type) {
    case MySQLColType::kString:
    case MySQLColType::kVarChar:
    case MySQLColType::kVarString:
    case MySQLColType::kEnum:
    case MySQLColType::kSet:
    case MySQLColType::kLongBlob:
    case MySQLColType::kMediumBlob:
    case MySQLColType::kBlob:
    case MySQLColType::kTinyBlob:
    case MySQLColType::kGeometry:
    case MySQLColType::kBit:
    case MySQLColType::kDecimal:
    case MySQLColType::kNewDecimal:
      PL_RETURN_IF_ERROR(DissectStringParam(msg, val_offset, &param->value));
      break;
    case MySQLColType::kTiny:
      PL_RETURN_IF_ERROR(DissectIntParam<1>(msg, val_offset, &param->value));
      break;
    case MySQLColType::kShort:
    case MySQLColType::kYear:
      PL_RETURN_IF_ERROR(DissectIntParam<2>(msg, val_offset, &param->value));
      break;
    case MySQLColType::kLong:
    case MySQLColType::kInt24:
      PL_RETURN_IF_ERROR(DissectIntParam<4>(msg, val_offset, &param->value));
      break;
    case MySQLColType::kLongLong:
      PL_RETURN_IF_ERROR(DissectIntParam<8>(msg, val_offset, &param->value));
      break;
    case MySQLColType::kFloat:
      PL_RETURN_IF_ERROR(DissectFloatParam<float>(msg, val_offset, &param->value));
      break;
    case MySQLColType::kDouble:
      PL_RETURN_IF_ERROR(DissectFloatParam<double>(msg, val_offset, &param->value));
      break;
    case MySQLColType::kDate:
    case MySQLColType::kDateTime:
    case MySQLColType::kTimestamp:
      PL_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, &param->value));
      break;
    case MySQLColType::kTime:
      PL_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, &param->value));
      break;
    case MySQLColType::kNull:
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
    LOG(WARNING) << absl::Substitute("Could not find prepare statement for stmt_id=$0", stmt_id);

    // We can't determine whether the rest of this packet is valid or not, so just return success.
    return ParseState::kSuccess;
  }

  int num_params = iter->second.response.header.num_params;

  size_t offset = kStmtIDStartOffset + kStmtIDBytes + kFlagsBytes + kIterationCountBytes;

  if (req_packet.msg.size() < offset + 1) {
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
      PL_RETURN_IF_ERROR(
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
    LOG(WARNING) << absl::Substitute("Cannot find Stmt Prepare Event to close [stmt_id=$0].",
                                     stmt_id);
  }

  return ParseState::kSuccess;
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
