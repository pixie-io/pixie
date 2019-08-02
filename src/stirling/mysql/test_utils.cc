#include <math.h>
#include <deque>
#include <string>

#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/utils/byte_format.h"

namespace pl {
namespace stirling {
namespace mysql {
namespace testutils {

/**
 * These Gen functions help generate raw string or Packet needed for testing the MySQL parser
 * or stitcher, respectively. The caller are expected to use structured events in test_data.h
 * and revert them back to strings or packets as test input.
 */

/**
 * Generates raw MySQL packets in the form of a string.
 */
std::string GenRawPacket(int packet_num, const std::string& msg) {
  char len_bytes[3];
  utils::IntToLEBytes<3>(msg.size(), len_bytes);
  return absl::StrCat(std::string(len_bytes, 3), packet_num, msg);
}

/**
 * Generates a raw packet with a string request.
 */
std::string GenRequest(ConstStrView command, const std::string& msg) {
  return GenRawPacket(0, absl::StrCat(command, msg));
}

/**
 * Generates the bytes of a length-encoded integer.
 * https://dev.mysql.com/doc/internals/en/integer.html#length-encoded-integer
 */
std::string GenLengthEncodedInt(int num) {
  DCHECK(num < pow(2, 64));
  if (num < 251) {
    char count_bytes[1];
    utils::IntToLEBytes<1>(num, count_bytes);
    return std::string(count_bytes, 1);
  } else if (num < pow(2, 16)) {
    char count_bytes[2];
    utils::IntToLEBytes<2>(num, count_bytes);
    return absl::StrCat("fc", std::string(count_bytes, 2));
  } else if (num < pow(2, 24)) {
    char count_bytes[3];
    utils::IntToLEBytes<3>(num, count_bytes);
    return absl::StrCat("fd", std::string(count_bytes, 3));
  } else {
    char count_bytes[8];
    utils::IntToLEBytes<8>(num, count_bytes);
    return absl::StrCat("fe", std::string(count_bytes, 8));
  }
}

/**
 * Generates the header packet of Resultset response. It contains num of cols.
 */
Packet GenCountPacket(int num_col) {
  std::string msg = GenLengthEncodedInt(num_col);
  return Packet{0, std::move(msg), MySQLEventType::kUnknown};
}

/**
 * Generates a Col Definition packet. Can be used in StmtPrepareResponse or Resultset.
 */
Packet GenColDefinition(const ColDefinition& col_def) {
  return Packet{0, std::move(col_def.msg), MySQLEventType::kUnknown};
}

/**
 * Generates a resultset row.
 */
Packet GenResultsetRow(const ResultsetRow& row) {
  return Packet{0, std::move(row.msg), MySQLEventType::kUnknown};
}

/**
 * Generates a header of StmtPrepare Response.
 */
Packet GenStmtPrepareRespHeader(const StmtPrepareRespHeader& header) {
  char statement_id[4];
  char num_columns[2];
  char num_params[2];
  char warning_count[2];
  utils::IntToLEBytes<4>(header.stmt_id, statement_id);
  utils::IntToLEBytes<2>(header.num_columns, num_columns);
  utils::IntToLEBytes<2>(header.num_params, num_params);
  utils::IntToLEBytes<2>(header.warning_count, warning_count);
  std::string msg =
      absl::StrCat(ConstStrView("\x00"), std::string(statement_id, 4), std::string(num_columns, 2),
                   std::string(num_params, 2), ConstStrView("\x00"), std::string(warning_count, 2));

  return Packet{0, std::move(msg), MySQLEventType::kUnknown};
}

/**
 * Generates a deque of packets. Contains a col counter packet and n resultset rows.
 */
std::deque<Packet> GenResultset(const Resultset& resultset) {
  std::deque<Packet> result;
  auto resp_header = GenCountPacket(resultset.num_col());
  result.emplace_back(std::move(resp_header));
  for (ColDefinition col_def : resultset.col_defs()) {
    result.emplace_back(GenColDefinition(col_def));
  }
  result.emplace_back(GenEOF());
  for (ResultsetRow row : resultset.results()) {
    result.emplace_back(GenResultsetRow(row));
  }
  result.emplace_back(GenEOF());
  return result;
}

/**
 * Generates a StmtPrepareOkResponse.
 */
std::deque<Packet> GenStmtPrepareOKResponse(const StmtPrepareOKResponse& resp) {
  std::deque<Packet> result;
  auto resp_header = GenStmtPrepareRespHeader(resp.resp_header());
  result.push_back(resp_header);

  for (ColDefinition param_def : resp.param_defs()) {
    ColDefinition p{std::move(param_def.msg)};
    result.push_back(GenColDefinition(p));
  }
  result.push_back(GenEOF());

  for (ColDefinition col_def : resp.col_defs()) {
    ColDefinition c{std::move(col_def.msg)};
    result.push_back(GenColDefinition(c));
  }
  result.push_back(GenEOF());
  return result;
}

Packet GenStmtExecuteRequest(const StmtExecuteRequest& req) {
  char statement_id[4];
  utils::IntToLEBytes<4>(req.stmt_id(), statement_id);
  std::string msg =
      absl::StrCat("\x17", std::string(statement_id, 4), ConstStrView("\x00\x01\x00\x00\x00"));
  int num_params = req.params().size();
  if (num_params > 0) {
    for (int i = 0; i < (num_params + 7) / 8; i++) {
      msg += std::string("\x00", 1);
    }
    msg += "\x01";
  }
  for (ParamPacket param : req.params()) {
    switch (param.type) {
      // TODO(chengruizhe): Add more types.
      case StmtExecuteParamType::kString:
        msg += std::string("\xfe\x00", 2);
        break;
      default:
        msg += std::string("\xfe\x00", 2);
        break;
    }
  }
  for (ParamPacket param : req.params()) {
    msg += GenLengthEncodedInt(param.value.size());
    msg += param.value;
  }
  return Packet{0, std::move(msg), MySQLEventType::kComStmtExecute};
}

/**
 * Generates a Err packet.
 */
Packet GenErr(const ErrResponse& err) {
  char error_code[2];
  utils::IntToLEBytes<2>(err.error_code(), error_code);
  std::string msg = absl::StrCat("\xff", std::string(error_code, 2), "\x23\x48\x59\x30\x30\x30",
                                 err.error_message());
  return Packet{0, std::move(msg), MySQLEventType::kUnknown};
}

/**
 * Generates a OK packet. Content is fixed.
 */
Packet GenOK() {
  std::string msg = std::string(ConstStrView("\x00\x00\x00\x02\x00\x00\x00"));
  return Packet{0, std::move(msg), MySQLEventType::kUnknown};
}

/**
 * Generates a EOF packet. Content is fixed.
 */
Packet GenEOF() {
  std::string msg = std::string(ConstStrView("\xfe\x00\x00\x22\x00"));
  return Packet{0, std::move(msg), MySQLEventType::kUnknown};
}

}  // namespace testutils
}  // namespace mysql
}  // namespace stirling
}  // namespace pl
