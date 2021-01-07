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

/**
 * Parses the input str as the provided protobuf message type.
 * Essentially a wrapper around PBWireToText that is easier to use.
 *
 * @param str The raw message as a string.
 * @param pb The message type to parse as; if nullptr, then it is treated as the Empty message.
 * @return The parsed message.
 */
std::string ParsePB(std::string_view str, ::google::protobuf::Message* pb);

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
