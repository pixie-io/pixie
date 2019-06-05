#pragma once

#include <string>
#include <utility>

/**
 * A Constant Compile-Time String
 *
 * Inspired from https://gist.github.com/creative-quant/6aa863e1cb415cbb9056f3d86f23b2c4
 *
 * Essentially a constant char[N] array, stored as char* and size.
 */
class ConstStrView {
 private:
  const char* const p_;
  const size_t size_;

 public:
  template <std::size_t N>
  // NOLINTNEXTLINE: implicit constructor.
  constexpr ConstStrView(const char (&a)[N]) : p_(a), size_(N - 1) {}
  constexpr const char* get() const { return p_; }
  constexpr size_t size() const { return size_; }

  constexpr bool equals(const ConstStrView& other) const {
    if (size_ != other.size_) {
      return false;
    }

    for (uint32_t i = 0; i < size_; i++) {
      if (p_[i] != other.p_[i]) {
        return false;
      }
    }

    return true;
  }
};

/**
 * A Constant Compile-Time Vector
 *
 * Essentially a constant T[N] type, stored as T* and size.
 */
template <class T>
class ConstVectorView {
 private:
  const T* const elements_;
  const size_t size_;

 public:
  constexpr ConstVectorView() : elements_(nullptr), size_(0) {}
  template <std::size_t N>
  // NOLINTNEXTLINE: implicit constructor.
  constexpr ConstVectorView(const T (&a)[N]) : elements_(a), size_(N) {}
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

// When used in a constexpr function, this will prevent compilation if assert does not pass.
#define COMPILE_TIME_ASSERT(expr, msg) (expr || error::Internal(#msg).ok())
