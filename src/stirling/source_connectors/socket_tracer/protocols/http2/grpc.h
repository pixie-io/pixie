#pragma once

#include <google/protobuf/message.h>

#include <optional>
#include <string>
#include <string_view>

namespace px {
namespace stirling {
namespace grpc {

constexpr size_t kGRPCMessageHeaderSizeInBytes = 5;

/**
 * Parses the input str as the provided protobuf message type.
 * Essentially a wrapper around PBWireToText that is easier to use.
 *
 * @param str The raw message as a string.
 * @param str_truncation_len The string length of any string/bytes fields beyond which truncation
 *        applies, if specified.
 * @return The parsed message.
 */
std::string ParsePB(std::string_view str, std::optional<int> str_truncation_len = std::nullopt);

}  // namespace grpc
}  // namespace stirling
}  // namespace px
