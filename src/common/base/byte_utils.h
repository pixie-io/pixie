#pragma once
#include <glog/logging.h>
#include <string>
#include <utility>

#include "src/common/base/base.h"

namespace pl {
namespace utils {

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

template <typename TCharType, size_t N>
void IntToLittleEndianByteStr(int num, TCharType (&result)[N]) {
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> (i * 8));
  }
}

}  // namespace utils
}  // namespace pl
