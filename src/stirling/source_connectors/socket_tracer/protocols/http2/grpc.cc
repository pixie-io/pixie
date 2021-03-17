#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"

#include <utility>
#include <vector>

#include <google/protobuf/empty.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include "src/common/base/base.h"
#include "src/stirling/utils/binary_decoder.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::google::protobuf::Empty;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;

namespace {

Status PBWireToText(std::string_view message, google::protobuf::Message* pb, std::string* text) {
  if (message.size() < kGRPCMessageHeaderSizeInBytes) {
    return error::InvalidArgument(
        "The gRPC message does not have enough data. "
        "Might be resulted from early termination of invalid RPC calls. "
        "E.g.: calling unimplemented method");
  }

  BinaryDecoder decoder(message);

  while (!decoder.eof()) {
    PL_ASSIGN_OR_RETURN(uint8_t compressed_flag, decoder.ExtractInt<uint8_t>());
    if (compressed_flag == 1) {
      return error::Unimplemented("Compressed data is not implemented");
    }
    PL_ASSIGN_OR_RETURN(uint32_t len, decoder.ExtractInt<uint32_t>());
    PL_ASSIGN_OR_RETURN(std::string_view data, decoder.ExtractString<char>(len));

    const bool succeeded = pb->ParseFromArray(data.data(), data.size());
    if (!succeeded) {
      return error::InvalidArgument("Failed to parse the serialized protobuf message");
    }

    // Using ::google::protobuf::util::MessageToJsonString() to print JSON string.
    // That requires the message to be of the actual type, not Empty.
    std::string message;
    if (!TextFormat::PrintToString(*pb, &message)) {
      return error::InvalidArgument("Failed to print protobuf message to text format");
    }
    text->append(message);
  }
  return Status::OK();
}

}  // namespace

// TODO(yzhao): Support reflection to get message types instead of empty message.
std::string ParsePB(std::string_view str) {
  Empty empty;
  std::string text;
  Status s = PBWireToText(str, &empty, &text);
  absl::StripTrailingAsciiWhitespace(&text);

  return s.ok() ? text : "<Failed to parse protobuf>";
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
