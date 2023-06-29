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

#include <cstring>
#include <string>
#include <utility>

#include "src/common/base/logging.h"

namespace px {
namespace utils {

using u8string = std::basic_string<uint8_t>;
using u8string_view = std::basic_string_view<uint8_t>;

template <size_t N>
void ReverseBytes(const uint8_t* x, uint8_t* y) {
  for (size_t k = 0; k < N; k++) {
    y[k] = x[N - k - 1];
  }
}

template <typename TCharType, size_t N>
void ReverseBytes(const TCharType (&x)[N], TCharType (&y)[N]) {
  const uint8_t* x_bytes = reinterpret_cast<const uint8_t*>(x);
  uint8_t* y_bytes = reinterpret_cast<uint8_t*>(y);
  ReverseBytes<N>(x_bytes, y_bytes);
}

template <typename T>
T ReverseBytes(const T* x) {
  T y;
  const uint8_t* x_bytes = reinterpret_cast<const uint8_t*>(x);
  uint8_t* y_bytes = reinterpret_cast<uint8_t*>(&y);
  ReverseBytes<sizeof(T)>(x_bytes, y_bytes);
  return y;
}

/**
 * Convert a little-endian string of bytes to an integer.
 *
 * @tparam T The receiver int type.
 * @tparam N Number of bytes to process from the source buffer. N must be <= sizeof(T).
 * If N < sizeof(T), the remaining bytes (MSBs) are assumed to be zero.
 * @param buf The sequence of bytes.
 * @return The decoded int value.
 */
template <typename T, typename TCharType = char, size_t N = sizeof(T)>
T LEndianBytesToIntInternal(std::basic_string_view<TCharType> buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  static_assert(N <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), N);

  T result = 0;
  for (size_t i = 0; i < N; i++) {
    result = static_cast<uint8_t>(buf[N - 1 - i]) | (result << 8);
  }
  return result;
}

template <typename T, size_t N = sizeof(T)>
T LEndianBytesToInt(std::string_view buf) {
  return LEndianBytesToIntInternal<T, char, N>(buf);
}

template <typename T, size_t N = sizeof(T), typename TCharType = char>
T LEndianBytesToInt(std::basic_string_view<TCharType> buf) {
  return LEndianBytesToIntInternal<T, TCharType, N>(buf);
}

/**
 * Convert a little-endian string of bytes to a float/double.
 *
 * @tparam T The receiver float type.
 * @param buf The sequence of bytes.
 * @return The decoded float value.
 */
template <typename TFloatType>
TFloatType LEndianBytesToFloat(std::string_view buf) {
  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), sizeof(TFloatType));

  TFloatType val;
  std::memcpy(&val, buf.data(), sizeof(TFloatType));
  return val;
}

/**
 * Convert an int to a little-endian string of bytes.
 *
 * @tparam TCharType The char type to use in the string (e.g. char vs uint8_t).
 * @param num The number to convert.
 * @param result the destination buffer.
 */
template <typename TCharType, size_t N>
void IntToLEndianBytes(int64_t num, TCharType (&result)[N]) {
  static_assert(N <= sizeof(int64_t));
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> (i * 8));
  }
}

/**
 * Convert an int to a big-endian string of bytes.
 *
 * @tparam TCharType The char type to use in the string (e.g. char vs uint8_t).
 * @param num The number to convert.
 * @param result the destination buffer.
 */
template <typename TCharType, size_t N>
void IntToBEndianBytes(int64_t num, TCharType (&result)[N]) {
  static_assert(N <= sizeof(int64_t));
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> ((N - i - 1) * 8));
  }
}

/**
 * Convert a big-endian string of bytes to an integer.
 *
 * @tparam T The receiver int type.
 * @tparam N Number of bytes to process from the source buffer. N must be <= sizeof(T).
 * If N < sizeof(T), the remaining bytes (MSBs) are assumed to be zero.
 * @param buf The sequence of bytes.
 * @return The decoded int value.
 */
template <typename T, typename TCharType, size_t N = sizeof(T)>
T BEndianBytesToIntInternal(std::basic_string_view<TCharType> buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  static_assert(N <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), N);

  T result = 0;
  for (size_t i = 0; i < N; i++) {
    result = static_cast<uint8_t>(buf[i]) | (result << 8);
  }
  return result;
}

template <typename T, size_t N = sizeof(T)>
T BEndianBytesToInt(std::string_view buf) {
  return BEndianBytesToIntInternal<T, char, N>(buf);
}

template <typename T, size_t N = sizeof(T), typename TCharType = char>
T BEndianBytesToInt(std::basic_string_view<TCharType> buf) {
  return BEndianBytesToIntInternal<T, TCharType, N>(buf);
}

/**
 * Convert a big-endian string of bytes to a float/double.
 *
 * @tparam T The receiver float type.
 * @param buf The sequence of bytes.
 * @return The decoded float value.
 */
template <typename TFloatType>
TFloatType BEndianBytesToFloat(std::string_view buf) {
  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), sizeof(TFloatType));

  // Note that unlike LEndianBytesToFloat, we don't use memcpy to align the data.
  // That is because ReverseBytes will naturally cause the alignment to occur when it copies bytes.
  const TFloatType* ptr = reinterpret_cast<const TFloatType*>(buf.data());
  return ReverseBytes<TFloatType>(ptr);
}

template <typename TValueType>
TValueType MemCpy(const void* buf) {
  TValueType tmp;
  memcpy(&tmp, buf, sizeof(tmp));
  return tmp;
}

template <typename TValueType, typename TByteType>
TValueType MemCpy(std::basic_string_view<TByteType> buf) {
  static_assert(sizeof(TByteType) == 1);
  return MemCpy<TValueType>(static_cast<const void*>(buf.data()));
}

template <typename TValueType, typename TByteType>
TValueType MemCpy(const TByteType* buf) {
  static_assert(sizeof(TByteType) == 1);
  return MemCpy<TValueType>(static_cast<const void*>(buf));
}

}  // namespace utils
}  // namespace px
