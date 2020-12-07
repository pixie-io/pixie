#include "src/stirling/protocols/redis/parse.h"

#include "src/stirling/common/binary_decoder.h"
#include "src/stirling/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

size_t FindMessageBoundary(MessageType /*type*/, std::string_view /*buf*/, size_t /*start_pos*/) {
  return 0;
}

// Redis protocol specification: https://redis.io/topics/protocol
ParseState ParseMessage(std::string_view* buf, Message* msg) {
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR(const char first_char, decoder.ExtractChar(), return ParseState::kInvalid);

  if (first_char == kSimpleStringMarker) {
    msg->data_type = DataType::kSimpleString;
    PL_ASSIGN_OR(msg->payload, decoder.ExtractStringUntil(kTerminalSequence),
                 return ParseState::kInvalid);
  } else if (first_char == kErrorMarker) {
    msg->data_type = DataType::kErrors;
    PL_ASSIGN_OR(msg->payload, decoder.ExtractStringUntil(kTerminalSequence),
                 return ParseState::kInvalid);
  }

  *buf = decoder.Buf();

  return ParseState::kSuccess;
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
