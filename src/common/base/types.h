#pragma once

#include <string>
#include <utility>

namespace pl {

/**
 * A helper function for converting c-style char arrays into string_views,
 * without having to explicitly specify the length.
 *
 * Only really required when there are null characters in the array,
 * where the regular std::string_view constructor only goes to the null character.
 * But can be used in other cases as well, without harm.
 */
template <size_t N>
inline constexpr std::string_view ConstStringView(const char (&a)[N]) {
  return std::string_view(a, N - 1);
}

template <size_t N>
inline std::string ConstString(const char (&a)[N]) {
  return std::string(a, N - 1);
}

template <size_t N>
inline constexpr std::string_view CharArrayStringView(const char (&a)[N]) {
  return std::string_view(a, N);
}

/**
 * A view into an array, with vector-like interface.
 * Similar to how string_view is a view into a string.
 * Essentially a view into a T[N] array, stored as T* and size.
 *
 * Mostly meant for use with constexpr c-style arrays.
 */
// TODO(oazizi): Investigate switching to std::span once we have c++20.
template <class T>
class ArrayView {
 private:
  const T* const elements_;
  const size_t size_;

 public:
  constexpr ArrayView() : elements_(nullptr), size_(0) {}
  template <std::size_t N>
  // NOLINTNEXTLINE: implicit constructor.
  constexpr ArrayView(const T (&a)[N]) : elements_(a), size_(N) {}
  constexpr ArrayView(const T* ptr, size_t size) : elements_(ptr), size_(size) {}
  constexpr size_t size() const { return size_; }
  constexpr const T& operator[](size_t i) const { return elements_[i]; }

  class iterator {
   public:
    // NOLINTNEXTLINE: implicit constructor.
    iterator(const T* ptr) : ptr(ptr) {}
    iterator operator++() {
      ++ptr;
      return *this;
    }
    bool operator!=(const iterator& other) const { return ptr != other.ptr; }
    const T& operator*() const { return *ptr; }

   private:
    const T* ptr;
  };
  iterator begin() const { return iterator(elements_); }
  iterator end() const { return iterator(elements_ + size_); }
};

}  // namespace pl

// When used in a constexpr function, this will prevent compilation if assert does not pass.
#define COMPILE_TIME_ASSERT(expr, msg) (expr || error::Internal(#msg).ok())
