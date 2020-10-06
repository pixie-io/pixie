#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/stirling/common/parse_state.h"
#include "src/stirling/common/protocol_traits.h"
#include "src/stirling/common/stitcher.h"
#include "src/stirling/dns/types.h"

namespace pl {
namespace stirling {
namespace dns {

/**
 * ProcessFrames is the entry point of the DNS Stitcher.
 *
 * @param req_packets: deque of all request frames.
 * @param resp_packets: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> ProcessFrames(std::deque<Frame>* req_packets,
                                            std::deque<Frame>* resp_packets);

}  // namespace dns

inline RecordsWithErrorCount<dns::Record> ProcessFrames(std::deque<dns::Frame>* req_packets,
                                                        std::deque<dns::Frame>* resp_packets,
                                                        NoState* /* state */) {
  return dns::ProcessFrames(req_packets, resp_packets);
}

}  // namespace stirling
}  // namespace pl
