#pragma once

#include <absl/strings/str_replace.h>
#include <string>

namespace px {
namespace grpc {

/**
 * @brief Transform the method path from gRPC format to protobuf format.
 */
// TODO(yzhao): We should remove this once https://github.com/grpc/grpc/issues/19618 is closed.
// By then there would be first class API from gRPC code base.
inline std::string MethodPath(std::string_view grpc_path) {
  while (grpc_path.front() == '/') {
    grpc_path.remove_prefix(1);
  }
  return absl::StrReplaceAll(grpc_path, {{"/", "."}});
}

}  // namespace grpc
}  // namespace px
