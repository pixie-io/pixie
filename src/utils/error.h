#pragma once

#include "absl/strings/substitute.h"

#include "src/utils/codes/error_codes.pb.h"
#include "src/utils/status.h"

namespace pl {
namespace error {

// Declare convenience functions:
// error::InvalidArgument(...)
// error::IsInvalidArgument(stat)
#define DECLARE_ERROR(FUNC, CONST)                                \
  template <typename... Args>                                     \
  Status FUNC(Args... args) {                                     \
    return Status(::pl::error::CONST, absl::Substitute(args...)); \
  }                                                               \
  inline bool Is##FUNC(const Status& status) { return status.code() == ::pl::error::CONST; }

DECLARE_ERROR(Cancelled, CANCELLED)
DECLARE_ERROR(Unknown, UNKNOWN)
DECLARE_ERROR(InvalidArgument, INVALID_ARGUMENT)
DECLARE_ERROR(DeadlineExceeded, DEADLINE_EXCEEDED)
DECLARE_ERROR(NotFound, NOT_FOUND)
DECLARE_ERROR(AlreadyExists, ALREADY_EXISTS)
DECLARE_ERROR(PermissionDenied, PERMISSION_DENIED)
DECLARE_ERROR(Unauthenticated, UNAUTHENTICATED)
DECLARE_ERROR(Internal, INTERNAL)
DECLARE_ERROR(Unimplmented, UNIMPLMENTED)

#undef DECLARE_ERROR

}  // namespace error
}  // namespace pl
