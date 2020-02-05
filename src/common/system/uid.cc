#include "src/common/system/uid.h"

#include <pwd.h>
#include <unistd.h>

#include <system_error>

#include "src/common/base/error.h"

namespace pl {

StatusOr<std::string> NameForUID(uid_t uid) {
  struct passwd pwd = {};
  struct passwd* result = nullptr;
  std::string buf;
  int rc = ERANGE;

  constexpr int kInitialBufSize = 256;
  constexpr int kMaximalBufSize = 16 * 1024;
  // Iteratively double buffer size until a limit. ERANGE indicates that the provided buffer size is
  // too small.
  for (int buf_size = kInitialBufSize; buf_size < kMaximalBufSize && rc == ERANGE; buf_size *= 2) {
    buf.resize(buf_size);
    rc = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);
  }
  if (rc != 0) {
    return error::System(std::error_code(rc, std::system_category()).message());
  }
  if (result == nullptr) {
    return error::NotFound("UID '$0' is not found", uid);
  }
  return std::string(pwd.pw_name);
}

}  // namespace pl
