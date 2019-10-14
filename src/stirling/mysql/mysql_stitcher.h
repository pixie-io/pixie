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
 * ProcessMySQLPackets is the entry point of the MySQL Stitcher. It loops through the req_packets,
 * parse their types, and calls the corresponding process functions that consume the corresponding
 * resp_packets and optionally produce an entry to emit.
 *
 * @param req_packets: deque of all request packets (requests are always single packets).
 * @param resp_packets: deque of all response packets (each request may have a 0, 1 or multiple
 * response packets).
 * @param state: MySQL state from previous requests (particularly state from prepared statements).
 * @return A vector of entries to be appended to table store,
 * or an error if an inconsistency was found that indicates we have lost track of the connection.
 */
StatusOr<std::vector<Entry>> ProcessMySQLPackets(std::deque<Packet>* req_packets,
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
 * @param entries: vector of entries that can be appended to if an event of interest is discovered.
 * @return There are three possible outcomes for each request (success, needs-more-data or error).
 *         Resp packets are only consumed on success. Needs-more-data simply indicates that
 *         not all response packets were present. Error means that an unexpected packet was
 * discovered, indicating that we have lost track of the connection.
 */
// TODO(oazizi): Convert these to StatusOr<ParseState>, since that adds clarity.
// Errors can still be returned through status instead of ParseState::kInvalid,
// because we can include an error message.

StatusOr<bool> ProcessStmtPrepare(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> ProcessStmtSendLongData(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                       mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> ProcessStmtExecute(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                  mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> ProcessStmtClose(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> ProcessStmtFetch(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> ProcessStmtReset(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                mysql::State* state, std::vector<Entry>* entries);

StatusOr<bool> ProcessQuery(const Packet& req_packet, std::deque<Packet>* resp_packets,
                            std::vector<Entry>* entries);

StatusOr<bool> ProcessFieldList(const Packet& req_packet, std::deque<Packet>* resp_packets,
                                std::vector<Entry>* entries);

StatusOr<bool> ProcessRequestWithBasicResponse(const Packet& req_packet,
                                               std::deque<Packet>* resp_packets,
                                               std::vector<Entry>* entries);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
