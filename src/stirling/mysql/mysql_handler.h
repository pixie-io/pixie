#pragma once
#include <deque>
#include <map>
#include <memory>

#include "src/common/base/statusor.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * Handlers are helper functions that transform MySQL Packets into request/response object.
 * MySQL Response can have one or more packets, so the functions pop off packets from the
 * deque as it parses the first packet.
 */
StatusOr<std::unique_ptr<ErrResponse>> HandleErrMessage(std::deque<Packet>* resp_packets);

StatusOr<std::unique_ptr<OKResponse>> HandleOKMessage(std::deque<Packet>* resp_packets);

StatusOr<std::unique_ptr<Resultset>> HandleResultset(std::deque<Packet>* resp_packets);

StatusOr<std::unique_ptr<StmtPrepareOKResponse>> HandleStmtPrepareOKResponse(
    std::deque<Packet>* resp_packets);

/**
 * MySQL Request can only have one packet, but StmtExecuteRequest is special. It needs to
 * look up the previously parsed StmtPrepare event based on a stmt_id when parsing the request.
 */
StatusOr<std::unique_ptr<StmtExecuteRequest>> HandleStmtExecuteRequest(
    const Packet& req_packet, std::map<int, ReqRespEvent>* prepare_map);

/**
 * StmtClose request contains the stmt_id of the prepare stmt to close. It simply deletes
 * the prepare stmt from the map (state of ConnectionTracker).
 */
Status HandleStmtCloseRequest(const Packet& req_packet, std::map<int, ReqRespEvent>* prepare_map);

/**
 * Other than StmtExecute request, all other requests are string requests.
 */
StatusOr<std::unique_ptr<StringRequest>> HandleStringRequest(const Packet& req_packet);
}  // namespace mysql
}  // namespace stirling
}  // namespace pl
