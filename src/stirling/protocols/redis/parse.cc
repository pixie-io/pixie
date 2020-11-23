#include "src/stirling/protocols/redis/parse.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

size_t FindMessageBoundary(MessageType /*type*/, std::string_view /*buf*/, size_t /*start_pos*/) {
  return 0;
}

// Redis protocol specification: https://redis.io/topics/protocol
ParseState ParseMessage(std::string_view* /*buf*/, Message* /*msg*/) { return {}; }

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
