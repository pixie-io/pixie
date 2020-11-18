#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/protocols/common/protocol_traits.h"
#include "src/stirling/protocols/common/stitcher.h"
#include "src/stirling/protocols/mysql/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace mysql {

/**
 * ProcessMySQLPackets is the entry point of the MySQL Stitcher. It loops through the req_packets,
 * parse their types, and calls the corresponding process functions that consume the corresponding
 * resp_packets and optionally produce an entry to emit.
 *
 * @param req_packets: deque of all request packets (requests are always single packets).
 * @param resp_packets: deque of all response packets (each request may have a 0, 1 or multiple
 * response packets).
 * @param state: MySQL state from previous requests (particularly state from prepared statements).
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> ProcessMySQLPackets(std::deque<Packet>* req_packets,
                                                  std::deque<Packet>* resp_packets,
                                                  mysql::State* state);

/**
 * The following process functions are helper functions that each processes a type of req_packet.
 *
 * @param req_packet: The request packet of the appropriate type.
 * @param resp_packets: The deque of all response packets. The head of the deque is the first
 * corresponding response packet for the request. A request may have 0, 1 or multiple response
 * packets.
 * @param state: MySQL state from previous "statement" requests (i.e. state from prepared
 * statements).
 * @param entry: entry where details of the request and responses are populated.
 * @return There are two possible normal outcomes for each request (success or needs-more-data),
 *         in addition to error cases. Resp packets are only consumed on success.
 *         Needs-more-data simply indicates that not all response packets were present.
 *         Error are communicated through Status, and indicate an unexpected packet.
 *         This usually means we have lost track of the connection.
 *         Note that errors are communicated through Status and include an error message.
 */

StatusOr<ParseState> ProcessStmtPrepare(const Packet& req_packet, DequeView<Packet> resp_packets,
                                        mysql::State* state, Record* entry);

StatusOr<ParseState> ProcessStmtSendLongData(const Packet& req_packet,
                                             DequeView<Packet> resp_packets, mysql::State* state,
                                             Record* entry);

StatusOr<ParseState> ProcessStmtExecute(const Packet& req_packet, DequeView<Packet> resp_packets,
                                        mysql::State* state, Record* entry);

StatusOr<ParseState> ProcessStmtClose(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      mysql::State* state, Record* entry);

StatusOr<ParseState> ProcessStmtFetch(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      mysql::State* state, Record* entry);

StatusOr<ParseState> ProcessStmtReset(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      mysql::State* state, Record* entry);

StatusOr<ParseState> ProcessQuery(const Packet& req_packet, DequeView<Packet> resp_packets,
                                  Record* entry);

StatusOr<ParseState> ProcessFieldList(const Packet& req_packet, DequeView<Packet> resp_packets,
                                      Record* entry);

StatusOr<ParseState> ProcessQuit(const Packet& req_packet, DequeView<Packet> resp_packets,
                                 Record* entry);

StatusOr<ParseState> ProcessRequestWithBasicResponse(const Packet& req_packet, bool string_req,
                                                     DequeView<Packet> resp_packets, Record* entry);

}  // namespace mysql

template <>
inline RecordsWithErrorCount<mysql::Record> StitchFrames(std::deque<mysql::Packet>* req_packets,
                                                         std::deque<mysql::Packet>* resp_packets,
                                                         mysql::StateWrapper* state) {
  return mysql::ProcessMySQLPackets(req_packets, resp_packets, &state->global);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
