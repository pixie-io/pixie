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

/**
 * @param num_col number of columns expected (parsed from header packet)
 * @param resp_packets deque of response packets to be checked
 * @param client_deprecate_eof_status whether client_deprecate_eof flag is set
 * This functions checks if the resultset is complete.
 */
Status CheckResultsetComplete(int num_col, std::deque<Packet>* resp_packets,
                              const FlagStatus& client_deprecate_eof_status) {
  bool client_deprecate_eof = client_deprecate_eof_status == FlagStatus::kSet;
  size_t expected_length = 1 + num_col + (client_deprecate_eof ? 0 : 1);
  if (resp_packets->size() < expected_length) {
    return error::Cancelled(
        "Handle Resultset: Not enough column definitions. Incomplete resultset.");
  }

  bool is_complete = false;

  // If it errors, an Err packet follows one or more resultset row packets.
  // If it doesn't error, in order to check if resultset is complete, we check that an EOF
  // packet exists after these packets. If client_deprecate_eof is set then an Ok is present.
  for (Packet p : *resp_packets) {
    bool ok_or_eof = client_deprecate_eof ? IsOKPacket(p) : IsEOFPacket(p);
    if (ok_or_eof || IsErrPacket(p)) {
      is_complete = true;
      break;
    }
  }
  if (!is_complete) {
    return error::Cancelled(
        "Handle Resultset: missing EOF after resultset rows. Incomplete resultset.");
  }
  return Status::OK();
}

/**
 * @param num_col number of columns expected (parsed from header packet)
 * @param resp_packets deque of response packets to be checked
 * @param state
 */
Status InferClientDeprecateEOF(int num_col, std::deque<Packet>* resp_packets,
                               FlagStatus* client_deprecate_eof_status) {
  size_t expected_length = 2 + num_col;
  if (resp_packets->size() < expected_length) {
    return error::Cancelled(
        "Handle Resultset: Not enough column definitions. Incomplete resultset.");
  }

  if (IsEOFPacket((*resp_packets)[num_col + 1])) {
    *client_deprecate_eof_status = FlagStatus::kNotSet;
  } else {
    *client_deprecate_eof_status = FlagStatus::kSet;
  }
  return Status::OK();
}

}  // namespace

//-----------------------------------------------------------------------------
// Message Level Functions
//-----------------------------------------------------------------------------

// TODO(chengruizhe): Move resp_packets->pop_front() out to the caller function and remove the arg.
StatusOr<std::unique_ptr<ErrResponse>> HandleErrMessage(std::deque<Packet>* resp_packets) {
  Packet packet = resp_packets->front();
  int error_code = utils::LEStrToInt(packet.msg.substr(1, 2));
  // TODO(chengruizhe): Assuming CLIENT_PROTOCOL_41 here. Make it more robust.
  // "\xff" + error_code[2] + sql_state_marker[1] + sql_state[5] (CLIENT_PROTOCOL_41) = 9
  // https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
  std::string err_message = packet.msg.substr(9);
  resp_packets->pop_front();
  return std::make_unique<ErrResponse>(ErrResponse(error_code, std::move(err_message)));
}

StatusOr<std::unique_ptr<OKResponse>> HandleOKMessage(std::deque<Packet>* resp_packets) {
  resp_packets->pop_front();
  return std::make_unique<OKResponse>(OKResponse());
}

StatusOr<std::unique_ptr<Resultset>> HandleResultset(std::deque<Packet>* resp_packets,
                                                     State* state) {
  Packet packet = resp_packets->front();

  int param_offset = 0;
  int num_col = ProcessLengthEncodedInt(packet.msg, &param_offset);
  if (num_col == 0) {
    return error::Cancelled("Handle Resultset: num of column is 0.");
  }

  if (state->client_deprecate_eof == FlagStatus::kUnknown) {
    PL_RETURN_IF_ERROR(
        InferClientDeprecateEOF(num_col, resp_packets, &state->client_deprecate_eof));
  }
  // header + col * n + eof(if n != 0 && !CLIENT_DEPRECATE_EOF) + result_set_row * m + eof(if
  // !CLIENT_DEPRECATE_EOF else ok)
  PL_RETURN_IF_ERROR(CheckResultsetComplete(num_col, resp_packets, state->client_deprecate_eof));

  // Pops header packet
  resp_packets->pop_front();

  std::vector<ColDefinition> col_defs;
  for (int i = 0; i < num_col; ++i) {
    if (IsEOFPacket(resp_packets->front())) {
      break;
    }
    Packet col_def_packet = resp_packets->front();
    ColDefinition col_def{col_def_packet.msg};
    col_defs.push_back(std::move(col_def));
    resp_packets->pop_front();
  }

  ProcessEOFPacket(resp_packets);

  std::vector<ResultsetRow> results;
  auto isLastPacket = [state, resp_packets](const Packet& p) {
    if (IsErrPacket(p)) {
      return true;
    }
    return state->client_deprecate_eof == FlagStatus::kSet ? IsOKPacket(resp_packets->front())
                                                           : IsEOFPacket(resp_packets->front());
  };

  while (!isLastPacket(resp_packets->front())) {
    Packet row_packet = resp_packets->front();
    ResultsetRow row{row_packet.msg};
    results.emplace_back(std::move(row));
    resp_packets->pop_front();
  }

  // TODO(chengruizhe): If it ends with err packet, handle the error and propagate up error_message.

  resp_packets->pop_front();
  return std::make_unique<Resultset>(Resultset(num_col, std::move(col_defs), std::move(results)));
}

StatusOr<std::unique_ptr<StmtPrepareOKResponse>> HandleStmtPrepareOKResponse(
    std::deque<Packet>* resp_packets) {
  Packet packet = resp_packets->front();
  LOG_IF(DFATAL, packet.msg.size() != 12)
      << "StmtPrepareOK response package message size must be 12.";
  int stmt_id = utils::LEStrToInt(packet.msg.substr(1, 4));
  size_t num_col = utils::LEStrToInt(packet.msg.substr(5, 2));
  size_t num_param = utils::LEStrToInt(packet.msg.substr(7, 2));
  size_t warning_count = utils::LEStrToInt(packet.msg.substr(10, 2));

  // TODO(chengruizhe): Handle missing packets more robustly. Assuming no missing packet.
  // If num_col or num_param is non-zero, they will be followed by EOF.
  // Reference: https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html.
  size_t expected_num_packets = 1 + num_col + num_param + (num_col != 0) + (num_param != 0);
  if (expected_num_packets > resp_packets->size()) {
    return error::Cancelled(
        "Handle StmtPrepareOKResponse: Not enough packets. Expected: %d. Actual:%d",
        expected_num_packets, resp_packets->size());
  }

  StmtPrepareRespHeader resp_header{stmt_id, num_col, num_param, warning_count};
  // Pops header packet
  resp_packets->pop_front();

  // Params come before columns
  std::vector<ColDefinition> param_defs;
  for (size_t i = 0; i < num_param; ++i) {
    Packet param_def_packet = resp_packets->front();
    ColDefinition param_def{param_def_packet.msg};
    param_defs.push_back(std::move(param_def));
    resp_packets->pop_front();
  }

  if (num_param != 0) {
    LOG_IF(DFATAL, !IsEOFPacket(resp_packets->front()))
        << "The first packet in response must be EOF.";
  }
  ProcessEOFPacket(resp_packets);

  std::vector<ColDefinition> col_defs;
  for (size_t i = 0; i < num_col; ++i) {
    Packet col_def_packet = resp_packets->front();
    ColDefinition col_def{col_def_packet.msg};
    col_defs.push_back(std::move(col_def));
    resp_packets->pop_front();
  }

  if (num_col != 0) {
    LOG_IF(DFATAL, !IsEOFPacket(resp_packets->front()))
        << "The first packet in response must be EOF.";
  }
  ProcessEOFPacket(resp_packets);

  return std::make_unique<StmtPrepareOKResponse>(
      StmtPrepareOKResponse(resp_header, std::move(col_defs), std::move(param_defs)));
}

StatusOr<std::unique_ptr<StringRequest>> HandleStringRequest(const Packet& req_packet) {
  return std::make_unique<StringRequest>(StringRequest(req_packet.msg.substr(1)));
}

StatusOr<std::unique_ptr<StmtExecuteRequest>> HandleStmtExecuteRequest(
    const Packet& req_packet, std::map<int, ReqRespEvent>* prepare_map) {
  int stmt_id = utils::LEStrToInt(req_packet.msg.substr(kStmtIDStartOffset, kStmtIDBytes));

  auto iter = prepare_map->find(stmt_id);
  if (iter == prepare_map->end()) {
    // There can be 2 possibilities in this case:
    // 1. The stitcher is confused/messed up and accidentally deleted wrong prepare event.
    // 2. Client sent a Stmt Exec for a deleted Stmt Prepare
    // We return -1 as stmt_id to indicate error and defer decision to the caller.
    return std::make_unique<StmtExecuteRequest>(StmtExecuteRequest(-1, {}));
  }

  auto prepare_resp = static_cast<StmtPrepareOKResponse*>(iter->second.response());

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
  // If stmt_bound = 1, assume no params.
  return std::make_unique<StmtExecuteRequest>(StmtExecuteRequest(stmt_id, std::move(params)));
}

Status HandleStmtCloseRequest(const Packet& req_packet, std::map<int, ReqRespEvent>* prepare_map) {
  int stmt_id = utils::LEStrToInt(req_packet.msg.substr(kStmtIDStartOffset, kStmtIDBytes));
  auto iter = prepare_map->find(stmt_id);
  if (iter == prepare_map->end()) {
    return error::Cancelled("Can not find Stmt Prepare Event to close.");
  }
  prepare_map->erase(iter);
  return Status::OK();
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
