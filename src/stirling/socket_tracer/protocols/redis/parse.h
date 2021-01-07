#pragma once

#include <string_view>

#include "src/stirling/socket_tracer/protocols/common/interface.h"
#include "src/stirling/socket_tracer/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

size_t FindMessageBoundary(MessageType type, std::string_view buf, size_t start_pos);

// Redis protocol specification: https://redis.io/topics/protocol
ParseState ParseMessage(std::string_view* buf, Message* msg);

}  // namespace redis

template <>
inline size_t FindFrameBoundary<redis::Message>(MessageType type, std::string_view buf,
                                                size_t start_pos) {
  return redis::FindMessageBoundary(type, buf, start_pos);
}

template <>
inline ParseState ParseFrame(MessageType /*type*/, std::string_view* buf, redis::Message* msg) {
  return redis::ParseMessage(buf, msg);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
