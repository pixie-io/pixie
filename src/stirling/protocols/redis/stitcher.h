#pragma once

#include <deque>

#include "src/stirling/protocols/common/interface.h"
#include "src/stirling/protocols/common/timestamp_stitcher.h"
#include "src/stirling/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {

template <>
inline RecordsWithErrorCount<redis::Record> StitchFrames(std::deque<redis::Message>* reqs,
                                                         std::deque<redis::Message>* resps,
                                                         NoState* /* state */) {
  // NOTE: This cannot handle Redis pipelining if there is any missing message.
  // See https://redis.io/topics/pipelining for Redis pipelining.
  return StitchMessagesWithTimestampOrder<redis::Record>(reqs, resps);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
