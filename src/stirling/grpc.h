#pragma once

#include <google/protobuf/message.h>

#include <string>
#include <string_view>

#include "src/common/base/status.h"

namespace pl {
namespace stirling {
namespace grpc {

constexpr size_t kGRPCMessageHeaderSizeInBytes = 5;

/**
 * @brief Parses the input message with the input dynamic protobuf message, and export it in JSON
 * format.
 */
Status ParseProtobuf(std::string_view message, google::protobuf::Message* pb_msg,
                     std::string* json);

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
