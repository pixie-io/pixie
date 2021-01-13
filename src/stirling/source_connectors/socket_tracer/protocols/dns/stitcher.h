#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace dns {

/**
 * StitchFrames is the entry point of the DNS Stitcher.
 *
 * @param req_packets: deque of all request frames.
 * @param resp_packets: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> StitchFrames(std::deque<Frame>* req_packets,
                                           std::deque<Frame>* resp_packets);

}  // namespace dns

template <>
inline RecordsWithErrorCount<dns::Record> StitchFrames(std::deque<dns::Frame>* req_packets,
                                                       std::deque<dns::Frame>* resp_packets,
                                                       NoState* /* state */) {
  return dns::StitchFrames(req_packets, resp_packets);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
