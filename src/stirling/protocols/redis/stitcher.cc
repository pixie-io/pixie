#include "src/stirling/protocols/redis/stitcher.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

RecordsWithErrorCount<redis::Record> StitchFrames(std::deque<redis::Message>* /*reqs*/,
                                                  std::deque<redis::Message>* /*resps*/) {
  return {};
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
