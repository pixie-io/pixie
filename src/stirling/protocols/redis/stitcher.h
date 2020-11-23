#pragma once

#include <deque>

#include "src/stirling/protocols/common/interface.h"
#include "src/stirling/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

RecordsWithErrorCount<redis::Record> StitchFrames(std::deque<redis::Message>* reqs,
                                                  std::deque<redis::Message>* resps);

}  // namespace redis

template <>
inline RecordsWithErrorCount<redis::Record> StitchFrames(std::deque<redis::Message>* reqs,
                                                         std::deque<redis::Message>* resps,
                                                         NoState* /* state */) {
  return redis::StitchFrames(reqs, resps);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
