#include "src/stirling/http2/grpc.h"

#include <google/protobuf/empty.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::google::protobuf::Empty;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageToJsonString;

Status PBWireToText(std::string_view message, PBTextFormat fmt, google::protobuf::Message* pb,
                    std::string* text) {
  if (message.size() < kGRPCMessageHeaderSizeInBytes) {
    return error::InvalidArgument(
        "The gRPC message does not have enough data. "
        "Might be resulted from early termination of invalid RPC calls. "
        "E.g.: calling unimplemented method");
  }
  const bool succeeded = pb->ParseFromArray(message.data() + kGRPCMessageHeaderSizeInBytes,
                                            message.size() - kGRPCMessageHeaderSizeInBytes);
  if (!succeeded) {
    return error::InvalidArgument("Failed to parse the serialized protobuf message");
  }

  const ::google::protobuf::util::Status status;
  switch (fmt) {
    case PBTextFormat::kText:
      if (!TextFormat::PrintToString(*pb, text)) {
        return error::InvalidArgument("Failed to print protobuf message to text format");
      }
      break;
    case PBTextFormat::kJSON:
      if (!MessageToJsonString(*pb, text).ok()) {
        return error::Internal(absl::StrCat("Failed to print protobuf message to JSON"));
      }
      break;
    default:
      DCHECK(false) << "Impossible, added to please GCC.";
  }
  return Status::OK();
}

std::string ParsePB(std::string_view str, Message* pb) {
  std::string text;
  Status s;
  Empty empty;
  if (pb == nullptr) {
    pb = &empty;
  }
  s = PBWireToText(str, PBTextFormat::kText, pb, &text);
  return s.ok() ? text
                : absl::Substitute("$0; original data in hex format: $1", s.ToString(),
                                   BytesToString<bytes_format::HexCompact>(str));
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
