#pragma once

#include <cassert>
#include <string>
#include <utility>

#include "src/utils/codes/error_codes.pb.h"
#include "src/utils/macros.h"
#include "src/utils/status.h"

namespace pl {

// Concept and some code borrowed
// from : tensorflow/stream_executor/lib/statusor.h
template <typename T>
class StatusOr {
  template <typename U>
  friend class StatusOr;

 public:
  // Construct a new StatusOr with Status::UNKNOWN status
  StatusOr() : status_(pl::error::UNKNOWN, "") {}

  StatusOr(const Status& status);  // NOLINT

  StatusOr(const T& value);  // NOLINT

  // Conversion copy constructor, T must be copy constructible from U
  template <typename U>
  StatusOr(const StatusOr<U>& other) : status_(other.status_), value_(other.value_) {}

  // Conversion assignment operator, T must be assignable from U
  template <typename U>
  StatusOr& operator=(const StatusOr<U>& other) {
    status_ = other.status_;
    value_ = other.value_;
    return *this;
  }

  // Rvalue-reference overloads of the other constructors and assignment
  // operators, to support move-only types and avoid unnecessary copying.
  StatusOr(T&& value);  // NOLINT

  // Move conversion operator to avoid unnecessary copy.
  // T must be assignable from U.
  // Not marked with explicit so the implicit conversion can happen.
  template <typename U>
  StatusOr(StatusOr<U>&& other)  // NOLINT
      : status_(std::move(other.status_)), value_(std::move(other.value_)) {}

  // Move assignment operator to avoid unnecessary copy.
  // T must be assignable from U
  template <typename U>
  StatusOr& operator=(StatusOr<U>&& other) {
    status_ = std::move(other.status_);
    value_ = std::move(other.value_);
    return *this;
  }

  // Returns a reference to our status. If this contains a T, then
  // returns Status::OK.
  const Status& status() const { return status_; }

  // Returns this->status().ok()
  bool ok() const { return status_.ok(); }

  // Returns this->status().code()
  pl::error::Code code() const { return status_.code(); }

  // Returns this->status().msg()
  std::string msg() const { return status_.msg(); }

  const T& ValueOrDie() const;
  T& ValueOrDie();

  void CheckValueNotNull(const T& value);

  // Moves the current value.
  T ConsumeValueOrDie();

  template <typename U>
  struct IsNull {
    // For non-pointer U, a reference can never be NULL.
    static inline bool IsValueNull(const U& t) {
      auto _ = t;
      return false;
    }
  };

  template <typename U>
  struct IsNull<U*> {
    static inline bool IsValueNull(const U* t) { return t == NULL; }
  };

 private:
  Status status_;
  T value_;
};

template <typename T>
StatusOr<T>::StatusOr(const T& value) : status_(), value_(value) {
  CheckValueNotNull(value);
}

template <typename T>
const T& StatusOr<T>::ValueOrDie() const {
  // TODO(zasgar): replace with CHECK.
  assert(status_.ok());
  return value_;
}

template <typename T>
T& StatusOr<T>::ValueOrDie() {
  // TODO(zasgar): replace with CHECK.
  if (!status_.ok()) {
    abort();
  }
  return value_;
}

template <typename T>
T StatusOr<T>::ConsumeValueOrDie() {
  // TODO(zasgar): replace with CHECK.
  if (!status_.ok()) {
    abort();
  }
  return std::move(value_);
}

template <typename T>
StatusOr<T>::StatusOr(const Status& status) : status_(status) {
  // TODO(zasgar): replace with CHECK.
  if (status.ok()) {
    abort();
  }
  if (status.ok()) {
    status_ = Status(pl::error::INTERNAL,
                     "Status::OK is not a valid constructor argument to StatusOr<T>");
  }
}

template <typename T>
StatusOr<T>::StatusOr(T&& value) : status_() {
  CheckValueNotNull(value);
  value_ = std::move(value);
}

template <typename T>
void StatusOr<T>::CheckValueNotNull(const T& value) {
  assert(!IsNull<T>::IsValueNull(value));
  if (IsNull<T>::IsValueNull(value)) {
    status_ =
        Status(pl::error::INTERNAL, "NULL is not a valid constructor argument to StatusOr<T*>");
  }
}

}  // namespace pl
