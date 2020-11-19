#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/stirling/protocols/common/interface.h"
#include "src/stirling/protocols/cql/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace cass {

/**
 * StitchFrames is the entry point of the Cassandra Stitcher. It loops through the resp_frames,
 * matches them with the corresponding req_frames, and optionally produces an entry to emit.
 *
 * @param req_frames: deque of all request frames.
 * @param resp_frames: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> StitchFrames(std::deque<Frame>* req_frames,
                                           std::deque<Frame>* resp_frames);

}  // namespace cass

template <>
inline RecordsWithErrorCount<cass::Record> StitchFrames(std::deque<cass::Frame>* req_frames,
                                                        std::deque<cass::Frame>* resp_frames,
                                                        NoState* /* state */) {
  return cass::StitchFrames(req_frames, resp_frames);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
