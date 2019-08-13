#pragma once
#include <deque>
#include <string>
#include <utility>
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {
namespace mysql {
namespace testutils {

std::string GenRawPacket(uint8_t packet_num, const std::string& msg);

std::string GenRequest(ConstStrView command, const std::string& msg);

Packet GenCountPacket(int num_col);

Packet GenColDefinition(const ColDefinition& col_def);

Packet GenResultsetRow(const ResultsetRow& row);

Packet GenStmtPrepareRespHeader(const StmtPrepareRespHeader& header);

Packet GenStmtExecuteRequest(const StmtExecuteRequest& req);

Packet GenStringRequest(const StringRequest& req, MySQLEventType type);

std::deque<Packet> GenResultset(const Resultset& resultset);

std::deque<Packet> GenStmtPrepareOKResponse(const StmtPrepareOKResponse& resp);

Packet GenErr(const ErrResponse& err);

Packet GenOK();

Packet GenEOF();

}  // namespace testutils
}  // namespace mysql
}  // namespace stirling
}  // namespace pl
