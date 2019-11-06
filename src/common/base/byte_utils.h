#pragma once
#include <glog/logging.h>
#include <string>
#include <utility>

#include "src/common/base/base.h"

namespace pl {
namespace utils {

/**
 * Convert a string of bytes to an integer, assuming little-endian ordering.
 *
 * The string must not be longer than the int type, otherwise behavior is undefined,
 * although a DCHECK will fire in debug mode.
 *
 * @tparam TIntType The receiver int type. Signed vs unsigned decode to the same raw bytes.
 * @param str The sequence of bytes.
 * @return The decoded int value.
 */
template <typename TIntType = uint32_t>
TIntType LittleEndianByteStrToInt(std::string_view str) {
  DCHECK(str.size() <= sizeof(TIntType));
  TIntType result = 0;
  for (size_t i = 0; i < str.size(); i++) {
    result = static_cast<uint8_t>(str[str.size() - 1 - i]) + (result << 8);
  }
  return result;
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
void IntToLittleEndianByteStr(int64_t num, TCharType (&result)[N]) {
  static_assert(N <= sizeof(int64_t));
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> (i * 8));
  }
}

}  // namespace utils
}  // namespace pl
