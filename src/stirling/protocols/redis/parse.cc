#include "src/stirling/protocols/redis/parse.h"

#include "src/stirling/common/binary_decoder.h"
#include "src/stirling/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

namespace {

constexpr char kSimpleStringMarker = '+';
constexpr char kErrorMarker = '-';
constexpr char kIntegerMarker = ':';
constexpr char kBulkStringsMarker = '$';
constexpr char kArrayMarker = '*';
// This is Redis' universal terminating sequence.
constexpr std::string_view kTerminalSequence = "\r\n";

// Bulk string is formatted as <length>\r\n<actual string, up to 512MB>\r\n
Status ParseBulkString(BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(std::string_view len_str, decoder->ExtractStringUntil(kTerminalSequence));

  // Length could be -1, which stands for NULL, and means the value is not set.
  // That's different than an empty string, which length is 0.
  // So here we initialize the value to -2.
  int len = -2;
  if (!absl::SimpleAtoi(len_str, &len)) {
    return error::InvalidArgument("Length string '$0' cannot be parsed as integer", len_str);
  }

  constexpr int kNullBulkStringLen = -1;
  if (len < kNullBulkStringLen) {
    return error::InvalidArgument("Length cannot be less than -1, got '$0'", len_str);
  }
  constexpr int kMaxLen = 512 * 1024 * 1024;
  if (len > kMaxLen) {
    return error::InvalidArgument("Length cannot be larger than 512MB, got '$0'", len_str);
  }

  if (len == kNullBulkStringLen) {
    constexpr std::string_view kNullBulkString = "<NULL>";
    msg->payload = kNullBulkString;
  } else {
    PL_ASSIGN_OR_RETURN(msg->payload, decoder->ExtractString(len + kTerminalSequence.size()));
    if (!absl::EndsWith(msg->payload, kTerminalSequence)) {
      return error::InvalidArgument("Bulk string should be terminated by '$0'", kTerminalSequence);
    }
    msg->payload.remove_suffix(kTerminalSequence.size());
  }

  return Status::OK();
}

}  // namespace

size_t FindMessageBoundary(MessageType /*type*/, std::string_view buf, size_t start_pos) {
  for (; start_pos < buf.size(); ++start_pos) {
    const char type_marker = buf[start_pos];
    if (type_marker == kSimpleStringMarker || type_marker == kErrorMarker ||
        type_marker == kIntegerMarker || type_marker == kBulkStringsMarker ||
        type_marker == kArrayMarker) {
      return start_pos;
    }
  }
  return std::string_view::npos;
}

// Redis protocol specification: https://redis.io/topics/protocol
ParseState ParseMessage(std::string_view* buf, Message* msg) {
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR(const char first_char, decoder.ExtractChar(), return ParseState::kInvalid);

  if (first_char == kSimpleStringMarker) {
    msg->data_type = DataType::kSimpleString;
    PL_ASSIGN_OR(msg->payload, decoder.ExtractStringUntil(kTerminalSequence),
                 return ParseState::kNeedsMoreData);
  } else if (first_char == kErrorMarker) {
    msg->data_type = DataType::kError;
    PL_ASSIGN_OR(msg->payload, decoder.ExtractStringUntil(kTerminalSequence),
                 return ParseState::kNeedsMoreData);
  } else if (first_char == kIntegerMarker) {
    msg->data_type = DataType::kInteger;
    // TODO(yzhao): Consider include integer value directly in Message.
    PL_ASSIGN_OR(msg->payload, decoder.ExtractStringUntil(kTerminalSequence),
                 return ParseState::kNeedsMoreData);
  } else if (first_char == kBulkStringsMarker) {
    msg->data_type = DataType::kBulkString;
    // Bulk string is formatted as <length>\r\n<actual string, up to 512MB>\r\n
    auto status = ParseBulkString(&decoder, msg);
    if (error::IsNotFound(status)) {
      return ParseState::kNeedsMoreData;
    }
    if (!status.ok()) {
      return ParseState::kInvalid;
    }
  } else if (first_char == kArrayMarker) {
    msg->data_type = DataType::kArray;
  } else {
    return ParseState::kInvalid;
  }

  *buf = decoder.Buf();

  return ParseState::kSuccess;
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
