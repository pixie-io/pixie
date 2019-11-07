#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_handler.h"
#include "src/stirling/mysql/mysql_stitcher.h"

namespace pl {
namespace stirling {
namespace mysql {

// TODO(oazizi): Move dissectors out to parse_utils.cc.

Status DissectStringParam(std::string_view msg, size_t* param_offset, ParamPacket* packet) {
  PL_ASSIGN_OR_RETURN(int param_length, ProcessLengthEncodedInt(msg, param_offset));
  if (msg.size() < *param_offset + param_length) {
    return error::Internal("Not enough bytes to dissect string param.");
  }
  packet->value = msg.substr(*param_offset, param_length);
  *param_offset += param_length;
  return Status::OK();
}

template <size_t length>
Status DissectIntParam(std::string_view msg, size_t* offset, ParamPacket* packet) {
  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect int param.");
  }
  packet->value =
      std::to_string(utils::LittleEndianByteStrToInt<int64_t>(msg.substr(*offset, length)));
  *offset += length;
  return Status::OK();
}

// Template instantiations to include in the object file.
// TODO(oazizi): Consider moving the definition into the header file to avoid these.
// On the other hand, benefit of keeping it this way is that these have specifically been tested.
template Status DissectIntParam<1>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectIntParam<2>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectIntParam<4>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectIntParam<8>(std::string_view msg, size_t* offset, ParamPacket* packet);

template <typename TFloatType>
Status DissectFloatParam(std::string_view msg, size_t* offset, ParamPacket* packet) {
  size_t length = sizeof(TFloatType);
  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect float param.");
  }
  packet->value =
      std::to_string(utils::LittleEndianByteStrToFloat<TFloatType>(msg.substr(*offset, length)));
  *offset += length;
  return Status::OK();
}

// Template instantiations to include in the object file.
template Status DissectFloatParam<float>(std::string_view msg, size_t* offset, ParamPacket* packet);
template Status DissectFloatParam<double>(std::string_view msg, size_t* offset,
                                          ParamPacket* packet);

Status DissectDateTimeParam(std::string_view msg, size_t* offset, ParamPacket* packet) {
  if (msg.size() < *offset + 1) {
    return error::Internal("Not enough bytes to dissect date/time param.");
  }

  uint8_t length = static_cast<uint8_t>(msg[*offset]);
  ++*offset;

  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect date/time param.");
  }
  packet->value = "MySQL DateTime rendering not implemented yet";
  *offset += length;
  return Status::OK();
}

// Spec on how to dissect params is here:
// https://dev.mysql.com/doc/internals/en/binary-protocol-value.html
//
// List of parameter types is followed by list of parameter values,
// so we have two offset pointers, one that points to current type position,
// and one that points to current value position
Status DissectParam(std::string_view msg, size_t* type_offset, size_t* val_offset,
                    ParamPacket* param) {
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
      PL_RETURN_IF_ERROR(DissectStringParam(msg, val_offset, param));
      break;
    case MySQLColType::kTiny:
      PL_RETURN_IF_ERROR(DissectIntParam<1>(msg, val_offset, param));
      break;
    case MySQLColType::kShort:
    case MySQLColType::kYear:
      PL_RETURN_IF_ERROR(DissectIntParam<2>(msg, val_offset, param));
      break;
    case MySQLColType::kLong:
    case MySQLColType::kInt24:
      PL_RETURN_IF_ERROR(DissectIntParam<4>(msg, val_offset, param));
      break;
    case MySQLColType::kLongLong:
      PL_RETURN_IF_ERROR(DissectIntParam<8>(msg, val_offset, param));
      break;
    case MySQLColType::kFloat:
      PL_RETURN_IF_ERROR(DissectFloatParam<float>(msg, val_offset, param));
      break;
    case MySQLColType::kDouble:
      PL_RETURN_IF_ERROR(DissectFloatParam<double>(msg, val_offset, param));
      break;
    case MySQLColType::kDate:
    case MySQLColType::kDateTime:
    case MySQLColType::kTimestamp:
      PL_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, param));
      break;
    case MySQLColType::kTime:
      PL_RETURN_IF_ERROR(DissectDateTimeParam(msg, val_offset, param));
      break;
    case MySQLColType::kNull:
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unexpected/unhandled column type $0", msg[*type_offset]);
  }

  return Status::OK();
}

//-----------------------------------------------------------------------------
// Message Level Functions
//-----------------------------------------------------------------------------

void HandleErrMessage(DequeView<Packet> resp_packets, Record* entry) {
  DCHECK(!resp_packets.empty());
  const Packet& packet = resp_packets.front();

  // TODO(chengruizhe): Assuming CLIENT_PROTOCOL_41 here. Make it more robust.
  // "\xff" + error_code[2] + sql_state_marker[1] + sql_state[5] (CLIENT_PROTOCOL_41) = 9
  // https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
  entry->resp.msg = packet.msg.substr(9);

  int error_code = utils::LittleEndianByteStrToInt(packet.msg.substr(1, 2));
  // TODO(oazizi): Add error code into resp msg.
  PL_UNUSED(error_code);

  entry->resp.status = MySQLRespStatus::kErr;
  entry->resp.timestamp_ns = packet.timestamp_ns;
}

void HandleOKMessage(DequeView<Packet> resp_packets, Record* entry) {
  DCHECK(!resp_packets.empty());
  entry->resp.status = MySQLRespStatus::kOK;
  entry->resp.timestamp_ns = resp_packets.front().timestamp_ns;
}

#define RETURN_NEEDS_MORE_DATA_IF_END(iter, resp_packets) \
  if (iter == resp_packets.end()) {                       \
    entry->resp.status = MySQLRespStatus::kUnknown;       \
    return ParseState::kNeedsMoreData;                    \
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

  int stmt_id = utils::LittleEndianByteStrToInt(first_resp_packet.msg.substr(1, 4));
  size_t num_col = utils::LittleEndianByteStrToInt(first_resp_packet.msg.substr(5, 2));
  size_t num_param = utils::LittleEndianByteStrToInt(first_resp_packet.msg.substr(7, 2));
  size_t warning_count = utils::LittleEndianByteStrToInt(first_resp_packet.msg.substr(10, 2));

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

void HandleStringRequest(const Packet& req_packet, Record* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.msg = req_packet.msg.substr(1);
  entry->req.timestamp_ns = req_packet.timestamp_ns;
}

void HandleNonStringRequest(const Packet& req_packet, Record* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.msg.clear();
  entry->req.timestamp_ns = req_packet.timestamp_ns;
}

namespace {
std::string CombinePrepareExecute(std::string_view stmt_prepare_request,
                                  const std::vector<ParamPacket>& params) {
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
}  // namespace

void HandleStmtExecuteRequest(const Packet& req_packet,
                              std::map<int, PreparedStatement>* prepare_map, Record* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.timestamp_ns = req_packet.timestamp_ns;

  int stmt_id =
      utils::LittleEndianByteStrToInt(req_packet.msg.substr(kStmtIDStartOffset, kStmtIDBytes));

  auto iter = prepare_map->find(stmt_id);
  if (iter == prepare_map->end()) {
    // There can be 2 possibilities in this case:
    // 1. The stitcher is confused/messed up and accidentally deleted wrong prepare event.
    // 2. Client sent a Stmt Exec for a deleted Stmt Prepare
    // We return -1 as stmt_id to indicate error and defer decision to the caller.
    LOG(WARNING) << absl::Substitute("Could not find prepare statement for stmt_id=$0", stmt_id);
    return;
  }

  int num_params = iter->second.response.header.num_params;

  int offset = kStmtIDStartOffset + kStmtIDBytes + kFlagsBytes + kIterationCountBytes;

  // This is copied directly from the MySQL spec.
  const int null_bitmap_length = (num_params + 7) / 8;
  offset += null_bitmap_length;
  uint8_t stmt_bound = req_packet.msg[offset];
  offset += 1;

  std::vector<ParamPacket> params;
  if (stmt_bound == 1) {
    // Offset to first param type and first param value respectively.
    // Each call to DissectParam will advance the two offsets to their next positions.
    size_t param_type_offset = offset;
    size_t param_val_offset = offset + 2 * num_params;

    for (int i = 0; i < num_params; ++i) {
      ParamPacket param;
      Status s = DissectParam(req_packet.msg, &param_type_offset, &param_val_offset, &param);
      LOG_IF(ERROR, !s.ok()) << s.msg();
      params.emplace_back(param);
    }
  }

  std::string_view stmt_prepare_request = iter->second.request;
  entry->req.msg = CombinePrepareExecute(stmt_prepare_request, params);
}

void HandleStmtCloseRequest(const Packet& req_packet, std::map<int, PreparedStatement>* prepare_map,
                            Record* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->req.cmd = DecodeCommand(req_packet.msg[0]);
  entry->req.msg = "";
  entry->req.timestamp_ns = req_packet.timestamp_ns;

  int stmt_id =
      utils::LittleEndianByteStrToInt(req_packet.msg.substr(kStmtIDStartOffset, kStmtIDBytes));
  auto iter = prepare_map->find(stmt_id);
  if (iter == prepare_map->end()) {
    // We may have missed the prepare statement (e.g. due to the missing start of connection
    // problem), but we can still process the close, and continue on. Just print a warning.
    LOG(WARNING) << absl::Substitute("Cannot find Stmt Prepare Event to close [stmt_id=$0].",
                                     stmt_id);
    return;
  }
  prepare_map->erase(iter);
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
