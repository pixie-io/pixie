#pragma once
#include <string>
#include <utility>
#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace utils {

// TODO(chengruizhe/oazizi): Move to common someday.

int LEStrToInt(const std::string_view str);

template <size_t N>
void ReverseBytes(const char (&bytes)[N], char (&result)[N]) {
  for (size_t k = 0; k < N; k++) {
    result[k] = bytes[N - k - 1];
  }
}

template <size_t N>
void IntToLEBytes(int num, char (&result)[N]) {
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> (i * 8));
  }
}
}  // namespace utils
}  // namespace stirling
}  // namespace pl
