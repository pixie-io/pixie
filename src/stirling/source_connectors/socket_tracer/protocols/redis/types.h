#pragma once

#include <string>
#include <string_view>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase

namespace px {
namespace stirling {
namespace protocols {
namespace redis {

// Represents a generic Redis message.
struct Message : public FrameBase {
  // Actual payload, not including the data type marker, and trailing \r\n.
  std::string payload;

  // Redis command, one of https://redis.io/commands.
  std::string_view command;

  // If true, indicates this is a published message from the server to all of the subscribed
  // clients.
  bool is_published_message = false;

  size_t ByteSize() const override { return payload.size() + command.size(); }

  std::string ToString() const {
    return absl::Substitute("base=[$0] payload=[$1] command=$2", FrameBase::ToString(), payload,
                            command);
  }
};

// Represents a pair of request and response messages.
struct Record {
  Message req;
  Message resp;
  bool role_swapped = false;
};

// Required by event parser interface.
struct ProtocolTraits {
  using frame_type = Message;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace px
