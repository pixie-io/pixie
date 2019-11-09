#pragma once
#include <deque>
#include <string>
#include <utility>
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/stirling/mysql/mysql_types.h"

namespace pl {
namespace stirling {
namespace mysql {
namespace testutils {

std::string GenLengthEncodedInt(int num);

std::string GenRawPacket(uint8_t packet_num, std::string_view msg);

std::string GenRawPacket(const Packet& packet);

std::string GenRequestPacket(MySQLEventType command, std::string_view msg);

Packet GenCountPacket(uint8_t seq_id, int num_col);

Packet GenColDefinition(uint8_t seq_id, const ColDefinition& col_def);

Packet GenResultsetRow(uint8_t seq_id, const ResultsetRow& row);

Packet GenStmtPrepareRespHeader(uint8_t seq_id, const StmtPrepareRespHeader& header);

Packet GenStmtExecuteRequest(const StmtExecuteRequest& req);

Packet GenStmtCloseRequest(const StmtCloseRequest& req);

Packet GenStringRequest(const StringRequest& req, MySQLEventType type);

Packet GenStringRequest(const StringRequest& req, char command);

std::deque<Packet> GenResultset(const Resultset& resultset, bool client_eof_deprecate = false);

std::deque<Packet> GenStmtPrepareOKResponse(const StmtPrepareOKResponse& resp);

Packet GenErr(uint8_t seq_id, const ErrResponse& err);

Packet GenOK(uint8_t seq_id);

Packet GenEOF(uint8_t seq_id);

}  // namespace testutils
}  // namespace mysql
}  // namespace stirling
}  // namespace pl
