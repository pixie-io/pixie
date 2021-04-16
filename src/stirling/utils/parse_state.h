#pragma once

#include <string>
#include <vector>

namespace px {
namespace stirling {

enum class ParseState {
  kUnknown,

  // The parse failed: data is invalid.
  // Input buffer consumed is not consumed and parsed output element is invalid.
  kInvalid,

  // The parse is partial: data appears to be an incomplete message.
  // Input buffer may be partially consumed and the parsed output element is not fully populated.
  kNeedsMoreData,

  // The parse succeeded, but the data is ignored.
  // Input buffer is consumed, but the parsed output element is invalid.
  kIgnored,

  // The parse succeeded, but indicated the end-of-stream.
  // Input buffer is consumed, and the parsed output element is valid.
  // however, caller should stop parsing any future data on this stream, even if more data exists.
  // Use cases include messages that indicate a change in protocol (see HTTP status 101).
  kEOS,

  // The parse succeeded.
  // Input buffer is consumed, and the parsed output element is valid.
  kSuccess,
};

}  // namespace stirling
}  // namespace px
