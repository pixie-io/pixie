#pragma once
#include <string>
#include <utility>

#include "src/common/base/base.h"

namespace pl {
namespace utils {

/**
 * Convert a string of bytes to an integer, assuming bytes follow little-endian ordering.
 *
 * @tparam T The receiver int type. Signed vs unsigned decode to the same raw bytes.
 * @tparam N Number of bytes to process from the source buffer. N must be <= sizeof(T).
 * If N < sizeof(T), the remaining bytes are assumed to be zero.
 * @param buf The sequence of bytes.
 * @return The decoded int value.
 */
template <typename T, int N = sizeof(T)>
T LEndianBytesToInt(std::string_view buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  static_assert(N <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), N);

  T result = 0;
  for (size_t i = 0; i < N; i++) {
    result = static_cast<uint8_t>(buf[N - 1 - i]) + (result << 8);
  }
  return result;
}

template <typename TFloatType>
TFloatType LEndianBytesToFloat(std::string_view str) {
  DCHECK_EQ(str.size(), sizeof(TFloatType));
  return *reinterpret_cast<const TFloatType*>(str.data());
}

template <typename TCharType, size_t N>
void ReverseBytes(const TCharType (&bytes)[N], TCharType (&result)[N]) {
  for (size_t k = 0; k < N; k++) {
    result[k] = bytes[N - k - 1];
  }
}

/**
 * Convert an int to a string of bytes, assuming little-endian ordering.
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

}  // namespace utils
}  // namespace pl
