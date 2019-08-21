#include "src/stirling/grpc.h"

#include <google/protobuf/util/json_util.h>

#include "src/common/base/error.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::google::protobuf::Message;

Status ParseProtobuf(std::string_view message, Message* pb_msg, std::string* json) {
  if (pb_msg == nullptr) {
    return error::InvalidArgument("Missing the dynamic protobuf Message for parsing.");
  }
  if (message.size() < kGRPCMessageHeaderSizeInBytes) {
    return error::InvalidArgument(
        "The gRPC message does not have enough data. "
        "Might be resulted from early termination of invalid RPC calls. "
        "E.g.: calling unimplemented method.");
  }
  const bool succeeded = pb_msg->ParseFromArray(message.data() + kGRPCMessageHeaderSizeInBytes,
                                                message.size() - kGRPCMessageHeaderSizeInBytes);
  if (!succeeded) {
    return error::InvalidArgument("Failed to parse the serialized protobuf message.");
  }

  const ::google::protobuf::util::Status status =
      google::protobuf::util::MessageToJsonString(*pb_msg, json);
  if (!status.ok()) {
    std::string tmp;
    status.error_message().CopyToString(&tmp);
    return error::Internal(
        absl::StrCat("Failed to export protobuf message to JSON, error message: ", tmp));
  }
  return Status::OK();
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
