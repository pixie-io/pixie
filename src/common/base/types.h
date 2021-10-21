/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace px {

/**
 * A helper function for converting c-style char arrays into string_views,
 * without having to explicitly specify the length.
 *
 * Only really required when there are null characters in the array,
 * where the regular std::string_view constructor only goes to the null character.
 * But can be used in other cases as well, without harm.
 */
template <typename TCharType = char, size_t N>
inline constexpr std::basic_string_view<TCharType> ConstStringView(const TCharType (&a)[N]) {
  return std::basic_string_view<TCharType>(a, N - 1);
}

template <typename TCharType = char, size_t N>
inline std::basic_string<TCharType> ConstString(const char (&a)[N]) {
  return std::basic_string<TCharType>(reinterpret_cast<const TCharType*>(a), N - 1);
}

template <typename TCharType = char, size_t N>
inline constexpr std::basic_string_view<TCharType> CharArrayStringView(const TCharType (&a)[N]) {
  return std::basic_string_view<TCharType>(a, N);
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
  // NOLINTNEXTLINE: runtime/explicit
  constexpr ArrayView(const T (&a)[N]) : elements_(a), size_(N) {}
  constexpr ArrayView(const T* ptr, size_t size) : elements_(ptr), size_(size) {}
  template <std::size_t N>
  constexpr ArrayView(const std::array<T, N>& arr) : elements_(arr.data()), size_(arr.size()) {}

  constexpr size_t size() const { return size_; }
  constexpr const T& operator[](size_t i) const { return elements_[i]; }

  class iterator {
   public:
    // NOLINTNEXTLINE: runtime/explicit
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

template <typename T>
ArrayView<T> ToArrayView(const std::vector<T>& v) {
  return ArrayView(v.data(), v.size());
}

/**
 * A read-only view into an container, with std library like interface.
 * Similar to how string_view is a view into a string.
 */
// TODO(oazizi): Investigate switching to std::span once we have c++20.
template <typename T, template <typename, typename = std::allocator<T>> class TContainer>
class ContainerView {
 private:
  const TContainer<T>& vec_;
  size_t start_;
  size_t size_;

 public:
  // NOLINTNEXTLINE: runtime/explicit
  ContainerView(const TContainer<T>& vec) : vec_(vec), start_(0), size_(vec.size()) {}
  ContainerView(const TContainer<T>& vec, size_t start, size_t size)
      : vec_(vec), start_(start), size_(size) {}
  constexpr size_t size() const { return size_; }
  constexpr const T& operator[](size_t i) const { return vec_[start_ + i]; }
  typename TContainer<T>::const_iterator begin() const { return vec_.cbegin() + start_; }
  typename TContainer<T>::const_iterator end() const { return vec_.cbegin() + (start_ + size_); }
  const T& front() { return vec_[start_]; }
  void pop_front(size_t n = 1) {
    if (n > size_) {
      n = size_;
    }
    start_ += n;
    size_ -= n;
  }
  void pop_back(size_t n = 1) {
    if (n > size_) {
      n = size_;
    }
    size_ -= n;
  }
  void clear() { pop_front(size()); }
  bool empty() { return size_ == 0; }
};

template <typename T>
using VectorView = ContainerView<T, std::vector>;

template <typename T>
using DequeView = ContainerView<T, std::deque>;

struct __attribute__((packed)) int24_t {
  operator int() const { return data; }
  int24_t(int x) : data(x) {}
  int24_t() {}
  int32_t data : 24;
};

inline int operator<<(int24_t left, int shift) {
  left.data = (left.data << shift) & 0xffffff;
  return left.data;
}

struct __attribute__((packed)) uint24_t {
  operator int() const { return data; }
  uint24_t(int x) : data(x) {}
  uint24_t() {}
  uint32_t data : 24;
};

inline int operator<<(uint24_t left, int shift) {
  left.data = (left.data << shift) & 0xffffff;
  return left.data;
}

}  // namespace px

// When used in a constexpr function, this will prevent compilation if assert does not pass.
#define COMPILE_TIME_ASSERT(expr, msg) (expr || error::Internal(#msg).ok())
