#pragma once

#include <deque>
#include <vector>

namespace pl {
namespace stirling {
namespace protocols {

/**
 * Struct that should be the return type of ParseFrames() API in protocol pipeline stitchers.
 * @tparam TRecord Record type of the protocol.
 */
template <typename TRecord>
struct RecordsWithErrorCount {
  std::vector<TRecord> records;
  int error_count = 0;
};

/**
 * StitchFrames is the entry point of stitcher for all protocols. It loops through the resps,
 * matches them with the corresponding reqs, and returns stitched req & resp pairs as a list of
 * entries.
 *
 * @param reqs: deque of all request messages.
 * @param resps: deque of all response messages.
 * @return A vector of entries to be appended to table store.
 */
template <typename TRecordType, typename TFrameType, typename TStateType>
RecordsWithErrorCount<TRecordType> StitchFrames(std::deque<TFrameType>* reqs,
                                                std::deque<TFrameType>* resps, TStateType* state);

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
