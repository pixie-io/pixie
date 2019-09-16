#pragma once

#include <google/protobuf/message.h>

#include <string>
#include <string_view>

#include "src/common/base/status.h"

namespace pl {
namespace stirling {
namespace grpc {

constexpr size_t kGRPCMessageHeaderSizeInBytes = 5;

enum class PBTextFormat {
  // Corresponding to protobuf's native text format.
  kText,
  kJSON,
};

/**
 * @brief Parses the input message with the input dynamic protobuf message, and export it in JSON
 * format.
 */
Status PBWireToText(std::string_view message, PBTextFormat fmt, google::protobuf::Message* pb,
                    std::string* text);

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
