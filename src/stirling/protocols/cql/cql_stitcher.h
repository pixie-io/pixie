#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/stirling/common/parse_state.h"
#include "src/stirling/protocols/common/protocol_traits.h"
#include "src/stirling/protocols/common/stitcher.h"
#include "src/stirling/protocols/cql/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace cass {

/**
 * ProcessFrames is the entry point of the Cassandra Stitcher. It loops through the resp_packets,
 * matches them with the corresponding req_packets, and optionally produces an entry to emit.
 *
 * @param req_packets: deque of all request frames.
 * @param resp_packets: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> ProcessFrames(std::deque<Frame>* req_packets,
                                            std::deque<Frame>* resp_packets);

}  // namespace cass

inline RecordsWithErrorCount<cass::Record> ProcessFrames(std::deque<cass::Frame>* req_packets,
                                                         std::deque<cass::Frame>* resp_packets,
                                                         NoState* /* state */) {
  return cass::ProcessFrames(req_packets, resp_packets);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
