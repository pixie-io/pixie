#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"
#include "src/stirling/mysql/types.h"
#include "src/stirling/utils/req_resp_pair.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * The following functions check whether a Packet is of a certain type.
 */
bool IsEOFPacket(const Packet& packet);
bool IsErrPacket(const Packet& packet);
bool IsOKPacket(const Packet& packet);
bool IsResultsetRowPacket(const Packet& packet, bool client_deprecate_eof);
bool IsStmtPrepareOKPacket(const Packet& packet);

/**
 * The following functions process packets by attempting to parse through the fields and check
 * there's nothing extra.
 */
StatusOr<ColDefinition> ProcessColumnDefPacket(const Packet& packet);

/**
 * Checks an OK packet for the SERVER_MORE_RESULTS_EXISTS flag.
 */
bool MoreResultsExists(const Packet& last_packet);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
