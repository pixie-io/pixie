#include "src/stirling/socket_tracer/protocols/redis/parse.h"

#include <string>
#include <utility>
#include <vector>

#include "src/stirling/common/binary_decoder.h"
#include "src/stirling/socket_tracer/protocols/redis/types.h"

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

StatusOr<int> ParseSize(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(std::string_view size_str, decoder->ExtractStringUntil(kTerminalSequence));

  constexpr size_t kSizeStrMaxLen = 16;
  if (size_str.size() > kSizeStrMaxLen) {
    return error::InvalidArgument(
        "Redis size string is longer than $0, which indicates traffic is misclassified as Redis.",
        kSizeStrMaxLen);
  }

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

std::string Quote(std::string_view text) {
  constexpr std::string_view kQuotationMark = "\"";
  return absl::StrCat(kQuotationMark, text, kQuotationMark);
}

// Bulk string is formatted as <length>\r\n<actual string, up to 512MB>\r\n
Status ParseBulkString(BinaryDecoder* decoder, Message* msg) {
  msg->data_type = DataType::kBulkString;

  PL_ASSIGN_OR_RETURN(int len, ParseSize(decoder));

  constexpr int kMaxLen = 512 * 1024 * 1024;
  if (len > kMaxLen) {
    return error::InvalidArgument("Length cannot be larger than 512MB, got '$0'", len);
  }

  if (len == kNullSize) {
    constexpr std::string_view kNullBulkString = "<NULL>";
    msg->payload = kNullBulkString;
    return Status::OK();
  }

  PL_ASSIGN_OR_RETURN(std::string_view payload,
                      decoder->ExtractString(len + kTerminalSequence.size()));
  if (!absl::EndsWith(payload, kTerminalSequence)) {
    return error::InvalidArgument("Bulk string should be terminated by '$0'", kTerminalSequence);
  }
  payload.remove_suffix(kTerminalSequence.size());
  msg->payload = Quote(payload);
  return Status::OK();
}

// This calls ParseMessage(), which eventually calls ParseArray() and are both recursive functions.
// This is because Array message can include nested array messages.
Status ParseArray(BinaryDecoder* decoder, Message* msg);

Status ParseMessage(BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(const char type_marker, decoder->ExtractChar());

  switch (type_marker) {
    case kSimpleStringMarker: {
      msg->data_type = DataType::kSimpleString;
      PL_ASSIGN_OR_RETURN(std::string_view str, decoder->ExtractStringUntil(kTerminalSequence));
      msg->payload = Quote(str);
      break;
    }
    case kBulkStringsMarker: {
      PL_RETURN_IF_ERROR(ParseBulkString(decoder, msg));
      break;
    }
    case kErrorMarker: {
      msg->data_type = DataType::kError;
      PL_ASSIGN_OR_RETURN(std::string_view str, decoder->ExtractStringUntil(kTerminalSequence));
      msg->payload = Quote(str);
      break;
    }
    case kIntegerMarker: {
      msg->data_type = DataType::kInteger;
      // TODO(yzhao): Consider include integer value directly in Message.
      PL_ASSIGN_OR_RETURN(msg->payload, decoder->ExtractStringUntil(kTerminalSequence));
      break;
    }
    case kArrayMarker: {
      msg->data_type = DataType::kArray;
      PL_RETURN_IF_ERROR(ParseArray(decoder, msg));
      break;
    }
    default:
      return error::InvalidArgument("Unexpected Redis type marker char (displayed as integer): %d",
                                    type_marker);
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
    Message msg;
    PL_RETURN_IF_ERROR(ParseMessage(decoder, &msg));
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

  auto status = ParseMessage(&decoder, msg);

  if (!status.ok()) {
    return TranslateErrorStatus(status);
  }

  *buf = decoder.Buf();

  return ParseState::kSuccess;
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
