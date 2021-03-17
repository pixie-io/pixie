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
 * Parses the input str as the provided protobuf message type.
 * Essentially a wrapper around PBWireToText that is easier to use.
 *
 * @param str The raw message as a string.
 * @return The parsed message.
 */
std::string ParsePB(std::string_view str);

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
