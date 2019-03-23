#pragma once

#include "absl/strings/substitute.h"

#include "src/common/base/proto/status.pb.h"
#include "src/common/base/status.h"

namespace pl {
namespace error {

// Declare convenience functions:
// error::InvalidArgument(...)
// error::IsInvalidArgument(stat)
#define DECLARE_ERROR(FUNC, CONST)                                   \
  template <typename... Args>                                        \
  Status FUNC(Args... args) {                                        \
    return Status(::pl::statuspb::CONST, absl::Substitute(args...)); \
  }                                                                  \
  inline bool Is##FUNC(const Status& status) { return status.code() == ::pl::statuspb::CONST; }

DECLARE_ERROR(Cancelled, CANCELLED)
DECLARE_ERROR(Unknown, UNKNOWN)
DECLARE_ERROR(InvalidArgument, INVALID_ARGUMENT)
DECLARE_ERROR(DeadlineExceeded, DEADLINE_EXCEEDED)
DECLARE_ERROR(NotFound, NOT_FOUND)
DECLARE_ERROR(AlreadyExists, ALREADY_EXISTS)
DECLARE_ERROR(PermissionDenied, PERMISSION_DENIED)
DECLARE_ERROR(Unauthenticated, UNAUTHENTICATED)
DECLARE_ERROR(Internal, INTERNAL)
DECLARE_ERROR(Unimplemented, UNIMPLEMENTED)
DECLARE_ERROR(ResourceUnavailable, RESOURCE_UNAVAILABLE)

#undef DECLARE_ERROR

}  // namespace error
}  // namespace pl
