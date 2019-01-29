#pragma once

#include <memory>
#include <string>

#include "src/utils/codes/error_codes.pb.h"
#include "src/utils/macros.h"

namespace pl {

class PL_MUST_USE_RESULT Status {
 public:
  // Success status.
  Status() {}
  Status(const Status& s) noexcept;
  Status(pl::error::Code code, const std::string& msg);

  void operator=(const Status& s) noexcept;

  /// Returns true if the status indicates success.
  bool ok() const { return (state_ == NULL); }

  // Return self, this makes it compatible with StatusOr<>.
  const Status& status() const { return *this; }

  pl::error::Code code() const { return ok() ? pl::error::OK : state_->code; }

  const std::string& msg() const { return ok() ? empty_string() : state_->msg; }

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const;

  std::string ToString() const;

  static Status OK() { return Status(); }

 private:
  struct State {
    pl::error::Code code;
    std::string msg;
  };

  static const std::string& empty_string() {
    static std::string* empty = new std::string;
    return *empty;
  }

  // Will be null if status is OK.
  std::unique_ptr<State> state_;
};

inline Status::Status(const Status& s) noexcept
    : state_((s.state_ == NULL) ? NULL : new State(*s.state_)) {}

inline void Status::operator=(const Status& s) noexcept {
  // The following condition catches both aliasing (when this == &s),
  // and the common case where both s and *this are ok.
  if (state_ != s.state_) {
    if (s.state_.get() == nullptr) {
      state_ = nullptr;
    } else {
      state_ = std::unique_ptr<State>(new State(*s.state_));
    }
  }
}

inline bool Status::operator==(const Status& x) const {
  return (this->state_ == x.state_) || (ToString() == x.ToString());
}

inline bool Status::operator!=(const Status& x) const { return !(*this == x); }

inline Status StatusAdapter(const Status& s) noexcept { return Status(s); };

}  // namespace pl

// Early-returns the status if it is in error; otherwise, proceeds.
// The argument expression is guaranteed to be evaluated exactly once.
#define PL_RETURN_IF_ERROR(__status)                     \
  do {                                                   \
    ::pl::Status status = ::pl::StatusAdapter(__status); \
    if (!status.ok()) {                                  \
      return status;                                     \
    }                                                    \
  } while (false)

#define EXPECT_OK(value) EXPECT_TRUE((value).status().ok())

#define PL_CHECK_OK_PREPEND(to_call, msg)             \
  do {                                                \
    auto _s = (to_call);                              \
    CHECK(_s.ok()) << (msg) << ": " << _s.ToString(); \
  } while (false)

#ifdef NDEBUG
#define PL_DCHECK_OK(val) PL_UNUSED(val);
#else
#define PL_DCHECK_OK(val) PL_CHECK_OK_PREPEND(val, "Bad Status");
#endif

#define PL_CHECK_OK(val) PL_CHECK_OK_PREPEND(val, "Bad Status")
