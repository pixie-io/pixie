#pragma once
#include <string>
#include <utility>
#include "src/common/base/base.h"

namespace pl {
namespace utils {

int LEStrToInt(const std::string_view str);

template <typename TCharType, size_t N>
void ReverseBytes(const TCharType (&bytes)[N], TCharType (&result)[N]) {
  for (size_t k = 0; k < N; k++) {
    result[k] = bytes[N - k - 1];
  }
}

template <typename TCharType, size_t N>
void IntToLEBytes(int num, TCharType (&result)[N]) {
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> (i * 8));
  }
}

}  // namespace utils
}  // namespace pl
