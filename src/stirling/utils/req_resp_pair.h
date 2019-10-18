#pragma once

namespace pl {
namespace stirling {

// TODO(oazizi): Convert ReqRespPair to hold unique_ptrs to messages.
// TODO(yzhao/oazizi): Consider use of std::optional to indicate a non-existent request/response.
// Note that using unique_ptrs would obsolete this TODO.
template <typename TMessageType>
struct ReqRespPair {
  TMessageType req_message;
  TMessageType resp_message;
};

}  // namespace stirling
}  // namespace pl
