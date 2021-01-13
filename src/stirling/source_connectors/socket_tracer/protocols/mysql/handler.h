#pragma once
#include <deque>
#include <map>
#include <memory>

#include "src/common/base/statusor.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace mysql {

/**
 * Handlers are helper functions that transform MySQL Packets into request/response object.
 * MySQL Response can have one or more packets, so the functions pop off packets from the
 * deque as it parses the first packet.
 */
StatusOr<ParseState> HandleNoResponse(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      Record* entry);

StatusOr<ParseState> HandleErrMessage(DequeView<Packet> resp_packets, Record* entry);

StatusOr<ParseState> HandleOKMessage(DequeView<Packet> resp_packets, Record* entry);

/**
 * A Resultset can either be a binary resultset(returned by StmtExecute), or a text
 * resultset(returned by Query).
 */
StatusOr<ParseState> HandleResultsetResponse(DequeView<Packet> resp_packets, Record* entry,
                                             bool binaryresultset, bool multiresultset = false);

StatusOr<ParseState> HandleStmtPrepareOKResponse(DequeView<Packet> resp_packets, State* state,
                                                 Record* entry);

/**
 * MySQL Request can only have one packet, but StmtExecuteRequest is special. It needs to
 * look up the previously parsed StmtPrepare event based on a stmt_id when parsing the request.
 */
StatusOr<ParseState> HandleStmtExecuteRequest(const Packet& req_packet,
                                              std::map<int, PreparedStatement>* prepare_map,
                                              Record* entry);

/**
 * StmtClose request contains the stmt_id of the prepare stmt to close. It simply deletes
 * the prepare stmt from the map (state of ConnectionTracker).
 */
StatusOr<ParseState> HandleStmtCloseRequest(const Packet& req_packet,
                                            std::map<int, PreparedStatement>* prepare_map,
                                            Record* entry);

/**
 * Many requests have just a single string request (e.g. COM_QUERY).
 */
StatusOr<ParseState> HandleStringRequest(const Packet& req_packet, Record* entry);

/**
 * Some requests have more complex request bodies. Lump them in here for now.
 */
StatusOr<ParseState> HandleNonStringRequest(const Packet& req_packet, Record* entry);

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
