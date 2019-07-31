#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_handler.h"
#include "src/stirling/mysql/mysql_stitcher.h"
#include "src/stirling/utils/byte_format.h"

namespace pl {
namespace stirling {
namespace mysql {
namespace {

/**
 * Converts a length encoded int from string to int.
 * https://dev.mysql.com/doc/internals/en/integer.html#packet-Protocol::LengthEncodedInteger
 * If it is < 0xfb, treat it as a 1-byte integer.

 * If it is 0xfc, it is followed by a 2-byte integer.

 * If it is 0xfd, it is followed by a 3-byte integer.

 * If it is 0xfe, it is followed by a 8-byte integer.
 */
int ProcessLengthEncodedInt(const std::string s) {
  switch (s[0]) {
    case '\xfc':
    case '\xfd':
      return utils::LEStrToInt(s.substr(1));
    case '\xfe':
      CHECK_GE(static_cast<int>(s.size()), 8);
      return utils::LEStrToInt(s.substr(1));
  }
  return utils::LEStrToInt(s);
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

StatusOr<std::unique_ptr<Resultset>> HandleResultset(std::deque<Packet>* resp_packets) {
  Packet packet = resp_packets->front();
  int num_col = ProcessLengthEncodedInt(packet.msg);

  // TODO(chengruizhe): Add a cache so that don't need to iterate through to check if complete.
  // header + col * n + eof(if n != 0 && !CLIENT_DEPRECATE_EOF) + result_set_row * m + eof(if
  // !CLIENT_DEPRECATE_EOF else ok)
  // TODO(chengruizhe): Assuming that CLIENT_DEPRECATE_EOF is not set below. Make it robust.
  if (static_cast<int>(resp_packets->size()) < 2 + num_col) {  // Checks first EOF
    return error::Cancelled(
        "Handle Resultset: missing EOF after column definitions. Incomplete resultset.");
  }

  bool is_complete = false;

  if (num_col == 0) {
    return error::Cancelled("Handle Resultset: num of column is 0.");
  }

  // header[1] + col_def[num_col] + eof[1]. In order to check if
  // resultset is complete, we check that an EOF packet exists after these packets.
  for (size_t i = 2 + num_col; i < resp_packets->size(); ++i) {
    if (IsEOFPacket(resp_packets->at(i))) {
      is_complete = true;
      break;
    }
  }

  if (!is_complete) {
    return error::Cancelled(
        "Handle Resultset: missing EOF after resultset rows. Incomplete resultset.");
  }

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

  CHECK(IsEOFPacket(resp_packets->front()));
  ProcessEOFPacket(resp_packets);

  std::vector<ResultsetRow> results;
  while (!IsEOFPacket(resp_packets->front())) {
    Packet row_packet = resp_packets->front();
    ResultsetRow row{row_packet.msg};
    results.emplace_back(std::move(row));
    resp_packets->pop_front();
  }
  ProcessEOFPacket(resp_packets);

  return std::make_unique<Resultset>(Resultset(num_col, std::move(col_defs), std::move(results)));
}

StatusOr<std::unique_ptr<StmtPrepareOKResponse>> HandleStmtPrepareOKResponse(
    std::deque<Packet>* resp_packets) {
  Packet packet = resp_packets->front();
  CHECK_EQ(static_cast<int>(packet.msg.size()), 12);
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
    CHECK(IsEOFPacket(resp_packets->front()));
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
    CHECK(IsEOFPacket(resp_packets->front()));
  }
  ProcessEOFPacket(resp_packets);

  return std::make_unique<StmtPrepareOKResponse>(
      StmtPrepareOKResponse(resp_header, std::move(col_defs), std::move(param_defs)));
}

StatusOr<std::unique_ptr<StringRequest>> HandleStringRequest(const Packet& req_packet) {
  return std::make_unique<StringRequest>(StringRequest(req_packet.msg));
}

StatusOr<std::unique_ptr<StmtExecuteRequest>> HandleStmtExecuteRequest(
    const Packet& req_packet, std::map<int, ReqRespEvent>* prepare_map) {
  PL_UNUSED(req_packet);
  PL_UNUSED(prepare_map);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
