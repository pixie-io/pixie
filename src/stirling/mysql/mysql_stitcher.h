#pragma once
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/common/base/status.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * StitchMySQLPackets is the entry point of the MySQL Stitcher. It loops through the req_packets,
 * parse their types, and calls the corresponding Stitch functions.
 * @return A vector of entries to be appended to table store.
 */
StatusOr<std::vector<Entry>> StitchMySQLPackets(std::deque<Packet>* req_packets,
                                                std::deque<Packet>* resp_packets,
                                                mysql::State* state);

/**
 * The following stitch functions take req_packet, infer the type of the first response, and
 * calls handle functions to handle the request/response. It forms a ReqRespEvent and emits a
 * string. The prepare_events map maps stmt_id to stmt prepare event, such that stmt execute
 * event with the same stmt_id can look it up.
 * @return A entry in the table store.
 */
// TODO(oazizi): Convert these to StatusOr<ParseState>, since that adds clarity.
// Errors can still be returned through status instead of ParseState::kInvalid,
// because we can include an error message.
StatusOr<bool> StitchStmtPrepare(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                 mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> StitchStmtSendLongData(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                      mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> StitchStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                 mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> StitchStmtClose(const Packet& req_packet, std::deque<Packet>* resp_packets,
                               mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> StitchStmtFetch(const Packet& req_packet, std::deque<Packet>* resp_packets,
                               mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> StitchStmtReset(const Packet& req_packet, std::deque<Packet>* resp_packets,
                               mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> StitchQuery(const Packet& req_packet, std::deque<Packet>* resp_packets,
                           std::vector<Entry>* entries);

StatusOr<bool> StitchFieldList(const Packet& req_packet, std::deque<Packet>* resp_packets,
                               std::vector<Entry>* entries);

StatusOr<bool> StitchRequestWithBasicResponse(const Packet& req_packet,
                                              std::deque<Packet>* resp_packets,
                                              std::vector<Entry>* entries);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
