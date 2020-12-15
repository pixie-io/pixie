#include "src/stirling/protocols/redis/parse.h"

#include <string>
#include <utility>
#include <vector>

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
constexpr int kNullSize = -1;

// Bulk string is formatted as <length>\r\n<actual string, up to 512MB>\r\n
StatusOr<int> ParseSize(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(std::string_view size_str, decoder->ExtractStringUntil(kTerminalSequence));

  // Length could be -1, which stands for NULL, and means the value is not set.
  // That's different than an empty string, which length is 0.
  // So here we initialize the value to -2.
  int size = -2;
  if (!absl::SimpleAtoi(size_str, &size)) {
    return error::InvalidArgument("String '$0' cannot be parsed as integer", size_str);
  }

  if (size < kNullSize) {
    return error::InvalidArgument("Size cannot be less than $0, got '$1'", kNullSize, size_str);
  }

  return size;
}

// Bulk string is formatted as <length>\r\n<actual string, up to 512MB>\r\n
StatusOr<std::string_view> ParseBulkString(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(int len, ParseSize(decoder));

  constexpr int kMaxLen = 512 * 1024 * 1024;
  if (len > kMaxLen) {
    return error::InvalidArgument("Length cannot be larger than 512MB, got '$0'", len);
  }

  if (len == kNullSize) {
    constexpr std::string_view kNullBulkString = "<NULL>";
    return kNullBulkString;
  }

  PL_ASSIGN_OR_RETURN(std::string_view payload,
                      decoder->ExtractString(len + kTerminalSequence.size()));
  if (!absl::EndsWith(payload, kTerminalSequence)) {
    return error::InvalidArgument("Bulk string should be terminated by '$0'", kTerminalSequence);
  }
  payload.remove_suffix(kTerminalSequence.size());
  return payload;
}

Status ParseNonArray(const char type_marker, BinaryDecoder* decoder, Message* msg) {
  switch (type_marker) {
    case kSimpleStringMarker: {
      msg->data_type = DataType::kSimpleString;
      PL_ASSIGN_OR_RETURN(msg->payload, decoder->ExtractStringUntil(kTerminalSequence));
      break;
    }
    case kErrorMarker: {
      msg->data_type = DataType::kError;
      PL_ASSIGN_OR_RETURN(msg->payload, decoder->ExtractStringUntil(kTerminalSequence));
      break;
    }
    case kIntegerMarker: {
      msg->data_type = DataType::kInteger;
      // TODO(yzhao): Consider include integer value directly in Message.
      PL_ASSIGN_OR_RETURN(msg->payload, decoder->ExtractStringUntil(kTerminalSequence));
      break;
    }
    case kBulkStringsMarker: {
      msg->data_type = DataType::kBulkString;
      PL_ASSIGN_OR_RETURN(msg->payload, ParseBulkString(decoder));
      break;
    }
    default:
      LOG(DFATAL) << "Unexpected type marker: " << type_marker;
      break;
  }
  // This is needed for GCC build.
  return Status::OK();
}

// Array is formatted as *<size_str>\r\n[one of simple string, error, bulk string, etc.]
Status ParseArray(BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(int len, ParseSize(decoder));

  if (len == kNullSize) {
    constexpr std::string_view kNullArray = "[NULL]";
    msg->payload = kNullArray;
    return Status::OK();
  }

  std::vector<std::string> payloads;

  for (int i = 0; i < len; ++i) {
    PL_ASSIGN_OR_RETURN(const char first_char, decoder->ExtractChar());
    Message msg;
    PL_RETURN_IF_ERROR(ParseNonArray(first_char, decoder, &msg));
    payloads.push_back(std::move(msg.payload));
  }

  msg->payload = absl::StrCat("[", absl::StrJoin(payloads, ", "), "]");

  return Status::OK();
}

ParseState TranslateErrorStatus(const Status& status) {
  if (error::IsNotFound(status) || error::IsResourceUnavailable(status)) {
    return ParseState::kNeedsMoreData;
  }
  if (!status.ok()) {
    return ParseState::kInvalid;
  }
  DCHECK(false) << "Can only translate NotFound or ResourceUnavailable error, got: "
                << status.ToString();
  return ParseState::kSuccess;
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
// This can also be implemented as a recursive function.
ParseState ParseMessage(std::string_view* buf, Message* msg) {
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR(const char first_char, decoder.ExtractChar(), return ParseState::kInvalid);

  if (first_char == kArrayMarker) {
    msg->data_type = DataType::kArray;
    auto status = ParseArray(&decoder, msg);
    if (!status.ok()) {
      return TranslateErrorStatus(status);
    }
  } else if (first_char == kSimpleStringMarker || first_char == kErrorMarker ||
             first_char == kIntegerMarker || first_char == kBulkStringsMarker) {
    auto status = ParseNonArray(first_char, &decoder, msg);
    if (!status.ok()) {
      return TranslateErrorStatus(status);
    }
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
