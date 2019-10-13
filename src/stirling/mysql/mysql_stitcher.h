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
std::vector<Entry> StitchMySQLPackets(std::deque<Packet>* req_packets,
                                      std::deque<Packet>* resp_packets, mysql::State* state);

// TODO(chengruizhe): Can potentially templatize these functions, especially when we have more
// event types.
/**
 * The following stitch functions take req_packet, infer the type of the first response, and
 * calls handle functions to handle the request/response. It forms a ReqRespEvent and emits a
 * string. The prepare_events map maps stmt_id to stmt prepare event, such that stmt execute
 * event with the same stmt_id can look it up.
 * @return A entry in the table store.
 */
StatusOr<Entry> StitchStmtPrepare(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  mysql::State* state);

StatusOr<Entry> StitchStmtSendLongData(const Packet& req_packet);

StatusOr<Entry> StitchStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  mysql::State* state);

StatusOr<Entry> StitchStmtClose(const Packet& req_packet, mysql::State* state);

StatusOr<Entry> StitchStmtFetch(const Packet& req_packet, std::deque<Packet>* resp_packets);

StatusOr<Entry> StitchQuery(const Packet& req_packet, std::deque<Packet>* resp_packets);

StatusOr<Entry> StitchFieldList(const Packet& req_packet, std::deque<Packet>* resp_packets);

StatusOr<Entry> StitchRequestWithBasicResponse(const Packet& req_packet,
                                               std::deque<Packet>* resp_packets);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
