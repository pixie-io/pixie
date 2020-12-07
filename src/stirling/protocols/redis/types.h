#pragma once

#include <string_view>

#include "src/stirling/protocols/common/event_parser.h"  // For FrameBase

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

// Represents data types defined in the Redis protocol.
enum class DataType {
  kSimpleString,
  kErrors,
  kIntegers,
  kBulkStrings,
  kArrays,
};

constexpr char kSimpleStringMarker = '+';
constexpr char kErrorMarker = '-';
// This is Redis' universal terminating sequence.
constexpr std::string_view kTerminalSequence = "\r\n";

// Represents a generic Redis message.
struct Message : public FrameBase {
  // Specifies the data type.
  DataType data_type;

  // Actual payload, not including the data type marker, and trailing \r\n.
  std::string_view payload;

  size_t ByteSize() const override { return payload.size(); }
};

// Represents a pair of request and response messages.
struct Record {
  Message req;
  Message resp;
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
}  // namespace pl
