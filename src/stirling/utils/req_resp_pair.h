#pragma once

namespace pl {
namespace stirling {

// TODO(oazizi): Convert ReqRespPair to hold unique_ptrs to messages.
// TODO(yzhao/oazizi): Consider use of std::optional to indicate a non-existent request/response.
// Note that using unique_ptrs would obsolete this TODO.
template <typename TReqType, typename TRespType>
struct ReqRespPair {
  TReqType req;
  TRespType resp;
};

}  // namespace stirling
}  // namespace pl
