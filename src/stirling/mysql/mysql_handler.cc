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
namespace {

/**
 * Converts a length encoded int from string to int.
 * https://dev.mysql.com/doc/internals/en/integer.html#packet-Protocol::LengthEncodedInteger
 *
 * If it is < 0xfb, treat it as a 1-byte integer.
 * If it is 0xfc, it is followed by a 2-byte integer.
 * If it is 0xfd, it is followed by a 3-byte integer.
 * If it is 0xfe, it is followed by a 8-byte integer.
 */
int ProcessLengthEncodedInt(const std::string_view s, int* param_offset) {
  constexpr uint8_t kLencIntPrefix2b = 0xfc;
  constexpr uint8_t kLencIntPrefix3b = 0xfd;
  constexpr uint8_t kLencIntPrefix8b = 0xfe;

  int result;
  switch (static_cast<uint8_t>(s[*param_offset])) {
    case kLencIntPrefix2b:
      *param_offset += 1;
      result = utils::LEStrToInt(s.substr(*param_offset, 2));
      *param_offset += 2;
      break;
    case kLencIntPrefix3b:
      *param_offset += 1;
      result = utils::LEStrToInt(s.substr(*param_offset, 3));
      *param_offset += 3;
      break;
    case kLencIntPrefix8b:
      LOG_IF(DFATAL, s.size() >= 8) << "Input buffer size must be at least 8.";
      *param_offset += 1;
      result = utils::LEStrToInt(s.substr(*param_offset, 8));
      *param_offset += 8;
      break;
    default:
      result = utils::LEStrToInt(s.substr(*param_offset, 1));
      *param_offset += 1;
      break;
  }
  return result;
}

/**
 * Dissects String parameters
 *
 */
void DissectStringParam(const std::string_view msg, int* param_offset, ParamPacket* packet) {
  int param_length = ProcessLengthEncodedInt(msg, param_offset);
  packet->type = StmtExecuteParamType::kString;
  packet->value = msg.substr(*param_offset, param_length);
  *param_offset += param_length;
}

void DissectIntParam(const std::string_view msg, const char prefix, int* param_offset,
                     ParamPacket* packet) {
  StmtExecuteParamType type;
  size_t length;
  switch (prefix) {
    case kColTypeTiny:
      type = StmtExecuteParamType::kTiny;
      length = 1;
      break;
    case kColTypeShort:
      type = StmtExecuteParamType::kShort;
      length = 2;
      break;
    case kColTypeLong:
      type = StmtExecuteParamType::kLong;
      length = 4;
      break;
    case kColTypeLongLong:
      type = StmtExecuteParamType::kLongLong;
      length = 8;
      break;
    default:
      LOG(WARNING) << "DissectIntParam: Unknown param type";
      type = StmtExecuteParamType::kUnknown;
      length = 1;
      break;
  }
  packet->value = std::to_string(utils::LEStrToInt(msg.substr(*param_offset, length)));
  packet->type = type;
  *param_offset += length;
}

// TODO(chengruizhe): Currently dissecting unknown param as if it's a string. Make it more robust.
void DissectUnknownParam(const std::string_view msg, int* param_offset, ParamPacket* packet) {
  DissectStringParam(msg, param_offset, packet);
}

}  // namespace

//-----------------------------------------------------------------------------
// Message Level Functions
//-----------------------------------------------------------------------------

std::unique_ptr<ErrResponse> HandleErrMessage(DequeView<Packet> resp_packets) {
  DCHECK(!resp_packets.empty());
  const Packet& packet = resp_packets.front();
  int error_code = utils::LEStrToInt(packet.msg.substr(1, 2));
  // TODO(chengruizhe): Assuming CLIENT_PROTOCOL_41 here. Make it more robust.
  // "\xff" + error_code[2] + sql_state_marker[1] + sql_state[5] (CLIENT_PROTOCOL_41) = 9
  // https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
  std::string err_message = packet.msg.substr(9);

  return std::make_unique<ErrResponse>(error_code, std::move(err_message));
}

std::unique_ptr<OKResponse> HandleOKMessage(DequeView<Packet> resp_packets) {
  DCHECK(!resp_packets.empty());
  return std::make_unique<OKResponse>();
}

#define RETURN_NEEDS_MORE_DATA_IF_END(type, iter, resp_packets) \
  if (iter == resp_packets.end()) {                             \
    return std::unique_ptr<type>(nullptr);                      \
  }

StatusOr<std::unique_ptr<Resultset>> HandleResultsetResponse(DequeView<Packet> resp_packets) {
  auto iter = resp_packets.begin();

  VLOG(3) << absl::Substitute("HandleResultsetResponse with $0 packets", resp_packets.size());

  // Process header packet.
  RETURN_NEEDS_MORE_DATA_IF_END(Resultset, iter, resp_packets);
  const Packet& first_resp_packet = *iter;
  if (!IsLengthEncodedIntPacket(first_resp_packet)) {
    return error::Internal("First packet should be length-encoded integer.");
  }

  int param_offset = 0;
  int num_col = ProcessLengthEncodedInt(first_resp_packet.msg, &param_offset);
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
    return std::unique_ptr<Resultset>(nullptr);
  }

  // Go to next packet.
  ++iter;

  std::vector<ColDefinition> col_defs;
  for (int i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_END(Resultset, iter, resp_packets);

    if (!IsColumnDefPacket(*iter)) {
      return error::Internal("Expected column definition packet");
    }

    const Packet& col_def_packet = *iter;
    ColDefinition col_def{col_def_packet.msg};
    col_defs.push_back(std::move(col_def));
    ++iter;
  }

  // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
  bool client_deprecate_eof = true;
  RETURN_NEEDS_MORE_DATA_IF_END(Resultset, iter, resp_packets);
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
      return error::Internal(
          "Expected resultset row packet [OK=$0 ERR=$1 EOF=$2 client_deprecate_eof=$3]",
          IsOKPacket(row_packet), IsErrPacket(row_packet), IsEOFPacket(row_packet),
          client_deprecate_eof);
    }
    ResultsetRow row{row_packet.msg};
    results.emplace_back(std::move(row));
    ++iter;
  }

  RETURN_NEEDS_MORE_DATA_IF_END(Resultset, iter, resp_packets);

  const Packet& last_packet = *iter;
  if (IsErrPacket(last_packet)) {
    // TODO(chengruizhe/oazizi): If it ends with err packet, propagate up error_message.
    PL_UNUSED(last_packet);
  }

  ++iter;
  if (iter != resp_packets.end()) {
    LOG(ERROR) << absl::Substitute("Found $0 extra packets",
                                   std::distance(iter, resp_packets.end()));
  }

  return std::make_unique<Resultset>(Resultset(num_col, std::move(col_defs), std::move(results)));
}

StatusOr<std::unique_ptr<StmtPrepareOKResponse>> HandleStmtPrepareOKResponse(
    DequeView<Packet> resp_packets) {
  auto iter = resp_packets.begin();

  RETURN_NEEDS_MORE_DATA_IF_END(StmtPrepareOKResponse, iter, resp_packets);
  const Packet& first_resp_packet = *iter;
  if (!IsStmtPrepareOKPacket(first_resp_packet)) {
    return error::Internal("Expected StmtPrepareOK packet");
  }

  int stmt_id = utils::LEStrToInt(first_resp_packet.msg.substr(1, 4));
  size_t num_col = utils::LEStrToInt(first_resp_packet.msg.substr(5, 2));
  size_t num_param = utils::LEStrToInt(first_resp_packet.msg.substr(7, 2));
  size_t warning_count = utils::LEStrToInt(first_resp_packet.msg.substr(10, 2));

  // TODO(chengruizhe): Handle missing packets more robustly. Assuming no missing packet.
  // If num_col or num_param is non-zero, they will be followed by EOF.
  // Reference: https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html.
  size_t expected_num_packets = 1 + num_col + num_param + (num_col != 0) + (num_param != 0);
  if (expected_num_packets > resp_packets.size()) {
    return std::unique_ptr<StmtPrepareOKResponse>(nullptr);
  }

  StmtPrepareRespHeader resp_header{stmt_id, num_col, num_param, warning_count};
  // Pops header packet
  ++iter;

  // Params come before columns
  std::vector<ColDefinition> param_defs;
  for (size_t i = 0; i < num_param; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_END(StmtPrepareOKResponse, iter, resp_packets);

    const Packet& param_def_packet = *iter;
    ColDefinition param_def{param_def_packet.msg};
    param_defs.push_back(std::move(param_def));
    ++iter;
  }

  bool client_deprecate_eof = true;
  if (num_param != 0) {
    RETURN_NEEDS_MORE_DATA_IF_END(StmtPrepareOKResponse, iter, resp_packets);
    // Optional EOF packet, based on CLIENT_DEPRECATE_EOF.
    if (IsEOFPacket(*iter)) {
      ++iter;
      client_deprecate_eof = false;
    }
  }

  std::vector<ColDefinition> col_defs;
  for (size_t i = 0; i < num_col; ++i) {
    RETURN_NEEDS_MORE_DATA_IF_END(StmtPrepareOKResponse, iter, resp_packets);

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

  return std::make_unique<StmtPrepareOKResponse>(resp_header, std::move(col_defs),
                                                 std::move(param_defs));
}

void HandleStringRequest(const Packet& req_packet, Entry* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->cmd = DecodeCommand(req_packet.msg[0]);
  entry->req_msg = req_packet.msg.substr(1);
  entry->req_timestamp_ns = req_packet.timestamp_ns;
}

void HandleNonStringRequest(const Packet& req_packet, Entry* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->cmd = DecodeCommand(req_packet.msg[0]);
  entry->req_msg.clear();
  entry->req_timestamp_ns = req_packet.timestamp_ns;
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
                              std::map<int, PreparedStatement>* prepare_map, Entry* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->cmd = DecodeCommand(req_packet.msg[0]);
  entry->req_timestamp_ns = req_packet.timestamp_ns;

  int stmt_id = utils::LEStrToInt(req_packet.msg.substr(kStmtIDStartOffset, kStmtIDBytes));

  auto iter = prepare_map->find(stmt_id);
  if (iter == prepare_map->end()) {
    // There can be 2 possibilities in this case:
    // 1. The stitcher is confused/messed up and accidentally deleted wrong prepare event.
    // 2. Client sent a Stmt Exec for a deleted Stmt Prepare
    // We return -1 as stmt_id to indicate error and defer decision to the caller.
    LOG(WARNING) << absl::Substitute("Could not find prepare statement for stmt_id=$0", stmt_id);
    return;
  }

  StmtPrepareOKResponse* prepare_resp = iter->second.response.get();

  int num_params = prepare_resp->resp_header().num_params;

  int offset = kStmtIDStartOffset + kStmtIDBytes + kFlagsBytes + kIterationCountBytes;

  // This is copied directly from the MySQL spec.
  const int null_bitmap_length = (num_params + 7) / 8;
  offset += null_bitmap_length;
  uint8_t stmt_bound = req_packet.msg[offset];
  offset += 1;

  std::vector<ParamPacket> params;
  if (stmt_bound == 1) {
    int param_offset = offset + 2 * num_params;

    for (int i = 0; i < num_params; ++i) {
      uint8_t param_type = req_packet.msg[offset];
      offset += 2;

      ParamPacket param;
      switch (param_type) {
        // TODO(chengruizhe): Add more exec param types (short, long, float, double, datetime etc.)
        // https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnType
        case kColTypeNewDecimal:
        case kColTypeBlob:
        case kColTypeVarString:
        case kColTypeString:
          DissectStringParam(req_packet.msg, &param_offset, &param);
          break;
        case kColTypeTiny:
        case kColTypeShort:
        case kColTypeLong:
        case kColTypeLongLong:
          DissectIntParam(req_packet.msg, param_type, &param_offset, &param);
          break;
        default:
          DissectUnknownParam(req_packet.msg, &param_offset, &param);
          break;
      }
      params.emplace_back(param);
    }
  }

  std::string_view stmt_prepare_request = iter->second.request;
  entry->req_msg = CombinePrepareExecute(stmt_prepare_request, params);
}

void HandleStmtCloseRequest(const Packet& req_packet, std::map<int, PreparedStatement>* prepare_map,
                            Entry* entry) {
  DCHECK(!req_packet.msg.empty());
  entry->cmd = DecodeCommand(req_packet.msg[0]);
  entry->req_msg = "";
  entry->req_timestamp_ns = req_packet.timestamp_ns;

  int stmt_id = utils::LEStrToInt(req_packet.msg.substr(kStmtIDStartOffset, kStmtIDBytes));
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
