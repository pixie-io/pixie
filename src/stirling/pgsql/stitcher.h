#pragma once

#include <deque>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/common/stitcher.h"
#include "src/stirling/pgsql/types.h"

namespace pl {
namespace stirling {

namespace pgsql {

/**
 * Returns a formatted string for messages that can form the response for a query.
 * The input result argument begin is modified to point to the next message that has not been
 * examined yet.
 */
StatusOr<RegularMessage> AssembleQueryResp(MsgDeqIter* begin, const MsgDeqIter& end);

/**
 * Returns a list of RegularMessage corresponding to the Parse-bind-execute sequence request.
 */
StatusOr<std::vector<RegularMessage>> GetParseReqMsgs(MsgDeqIter* begin, const MsgDeqIter& end);

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps);

}  // namespace pgsql

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps,
                                                   NoState* /*state*/);

}  // namespace stirling
}  // namespace pl
