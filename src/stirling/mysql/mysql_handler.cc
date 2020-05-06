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

#define RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets) \
  if (resp_packets.empty()) {                         \
    entry->resp.status = MySQLRespStatus::kUnknown;   \
    return ParseState::kNeedsMoreData;                \
  }

StatusOr<ParseState> HandleNoResponse(DequeView<Packet> resp_packets, Record* entry) {
  if (!resp_packets.empty()) {
    return error::Internal("Did not expect any response packets [num_extra_packets=$0].",
                           resp_packets.size());
  }

  entry->resp.status = MySQLRespStatus::kNone;
  entry->resp.timestamp_ns = 0;

  return ParseState::kSuccess;
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

  if (resp_packets.size() > 1) {
    return error::Internal(
        "Did not expect additional packets after error packet [num_extra_packets=$0].",
        resp_packets.size() - 1);
  }

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

  if (resp_packets.size() > 1) {
    return error::Internal(
        "Did not expect additional packets after OK packet [num_extra_packets=$0].",
        resp_packets.size() - 1);
  }

  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleResultsetResponse(DequeView<Packet> resp_packets, Record* entry,
                                             bool multi_resultset) {
  VLOG(3) << absl::Substitute("HandleResultsetResponse with $0 packets", resp_packets.size());

  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  const Packet& first_resp_packet = resp_packets.front();
  resp_packets.pop_front();

  // The last resultset of a multi-resultset is just an OK packet.
  if (multi_resultset && IsOKPacket(first_resp_packet)) {
    entry->resp.status = MySQLRespStatus::kOK;
    entry->resp.timestamp_ns = first_resp_packet.timestamp_ns;
    LOG_IF(ERROR, resp_packets.size() != 1)
        << absl::Substitute("Found $0 extra packets", resp_packets.size() - 1);
    return ParseState::kSuccess;
  }

  // Process header packet.
  size_t param_offset = 0;
  auto s = ProcessLengthEncodedInt(first_resp_packet.msg, &param_offset);
  if (!s.ok()) {
    entry->resp.status = MySQLRespStatus::kUnknown;
    return error::Internal("Unable to process header packet of resultset response.");
  }
  int num_col = s.ValueOrDie();

  if (param_offset != first_resp_packet.msg.size()) {
    entry->resp.status = MySQLRespStatus::kUnknown;
    return error::Internal("Extra bytes in length-encoded int packet.");
  }

  if (num_col == 0) {
    entry->resp.status = MySQLRespStatus::kUnknown;
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
    entry->resp.status = MySQLRespStatus::kUnknown;
    return ParseState::kNeedsMoreData;
  }

  std::vector<ColDefinition> col_defs;
  for (int i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
    const Packet& packet = resp_packets.front();
    resp_packets.pop_front();

    auto s = ProcessColumnDefPacket(packet);
    if (!s.ok()) {
      entry->resp.status = MySQLRespStatus::kUnknown;
      return error::Internal("Expected column definition packet");
    }

    ColDefinition col_def = s.ValueOrDie();
    col_defs.push_back(std::move(col_def));
  }
  // TODO(chengruizhe): Use the type in col_def packets to parse the binary resultset row.

  // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
  bool client_deprecate_eof = true;
  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  if (IsEOFPacket(resp_packets.front())) {
    resp_packets.pop_front();
    client_deprecate_eof = false;
  }

  std::vector<ResultsetRow> results;

  auto isLastPacket = [client_deprecate_eof](const Packet& p) {
    // Depending on CLIENT_DEPRECATE_EOF, we may either get an OK or EOF packet.
    return IsErrPacket(p) || (client_deprecate_eof ? IsOKPacket(p) : IsEOFPacket(p));
  };

  while (!resp_packets.empty() && !isLastPacket(resp_packets.front())) {
    const Packet& row_packet = resp_packets.front();
    resp_packets.pop_front();
    if (!IsResultsetRowPacket(row_packet, client_deprecate_eof)) {
      entry->resp.status = MySQLRespStatus::kUnknown;
      return error::Internal(
          "Expected resultset row packet [OK=$0 ERR=$1 EOF=$2 client_deprecate_eof=$3]",
          IsOKPacket(row_packet), IsErrPacket(row_packet), IsEOFPacket(row_packet),
          client_deprecate_eof);
    }
    ResultsetRow row{row_packet.msg};
    results.emplace_back(std::move(row));
  }

  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  const Packet& last_packet = resp_packets.front();

  DCHECK(isLastPacket(resp_packets.front()));
  if (IsErrPacket(resp_packets.front())) {
    return HandleErrMessage(resp_packets, entry);
  }

  resp_packets.pop_front();

  if (multi_resultset) {
    absl::StrAppend(&entry->resp.msg, ", ");
  }
  absl::StrAppend(&entry->resp.msg, "Resultset rows = ", results.size());

  // Check for another resultset in case this is a multi-resultset.
  if (MoreResultsExists(last_packet)) {
    return HandleResultsetResponse(resp_packets, entry, true);
  }

  LOG_IF(ERROR, !resp_packets.empty())
      << absl::Substitute("Found $0 extra packets", resp_packets.size());

  entry->resp.status = MySQLRespStatus::kOK;
  entry->resp.timestamp_ns = last_packet.timestamp_ns;
  return ParseState::kSuccess;
}

StatusOr<ParseState> HandleStmtPrepareOKResponse(DequeView<Packet> resp_packets, State* state,
                                                 Record* entry) {
  RETURN_NEEDS_MORE_DATA_IF_EMPTY(resp_packets);
  const Packet& first_resp_packet = resp_packets.front();
  resp_packets.pop_front();
  if (!IsStmtPrepareOKPacket(first_resp_packet)) {
    entry->resp.status = MySQLRespStatus::kUnknown;
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
    entry->resp.status = MySQLRespStatus::kUnknown;
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
      entry->resp.status = MySQLRespStatus::kUnknown;
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
      entry->resp.status = MySQLRespStatus::kUnknown;
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

  entry->resp.status = MySQLRespStatus::kOK;
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
    LOG_FIRST_N(WARNING, 10) << absl::Substitute("Could not find prepare statement for stmt_id=$0",
                                                 stmt_id);

    // We can't determine whether the rest of this packet is valid or not, so just return success.
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
