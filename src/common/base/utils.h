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

#include <unistd.h>

#include <array>
#include <bitset>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <magic_enum.hpp>

#include <absl/strings/str_format.h>
#include "src/common/base/error.h"
#include "src/common/base/statusor.h"

namespace px {

inline bool IsRoot() { return (geteuid() == 0); }

// Implementation borrowed from: http://reedbeta.com/blog/python-like-enumerate-in-cpp17/
template <typename T, typename TIter = decltype(std::begin(std::declval<T>())),
          typename = decltype(std::end(std::declval<T>()))>
constexpr auto Enumerate(T&& iterable) {
  struct iterator {
    size_t i;
    TIter iter;
    bool operator!=(const iterator& other) const { return iter != other.iter; }
    void operator++() {
      ++i;
      ++iter;
    }
    auto operator*() const { return std::tie(i, *iter); }
  };
  struct iterable_wrapper {
    T iterable;
    auto begin() { return iterator{0, std::begin(iterable)}; }
    auto end() { return iterator{0, std::end(iterable)}; }
  };
  return iterable_wrapper{std::forward<T>(iterable)};
}

/**
 * An integer division that rounds up if there is any fractional portion.
 * This is in contrast to normal integer division (x/y) that removes the fractional part.
 */
template <typename TIntType>
constexpr TIntType IntRoundUpDivide(TIntType x, TIntType y) {
  return (x + (y - 1)) / y;
}

/**
 * Snap up to a multiple of the size.
 *   SnapUpToMultiple(64, 8) = 64
 *   SnapUpToMultiple(66, 8) = 72
 */
template <typename TIntType>
constexpr TIntType SnapUpToMultiple(TIntType x, TIntType size) {
  return IntRoundUpDivide(x, size) * size;
}

/**
 * Rounds an integer up to the next closest power of 2.
 * If already a power of 2, returns the same value.
 */
template <typename TIntType>
constexpr TIntType IntRoundUpToPow2(TIntType x) {
  TIntType power = 1;
  while (power < x) {
    power *= 2;
  }
  return power;
}

/**
 * Interpolate the y value at x=`value` along the line defined by the points (`x_a`, `y_a`) (`x_b`,
 * `y_b`). If `value` falls outside [`x_a`, `x_b`] this function will extrapolate. If `x_a` equals
 * `x_b` the behaviour is undefined, so we arbtrarily choose to return `y_a` in this case. Note that
 * `x_a` need not be less than `x_b`, the interpolation is symmetrical around a swap of point a and
 * point b.
 * @tparam TXIntType Integer Type for the x-values.
 * @tparam TYIntType Integer Type for the y-values. Note that TYIntType must be a signed integer
 * type. An unsigned integer type will lead to undefined behaviour.
 * @param x_a x coordinate of first point in the line.
 * @param x_b x coordinate of second point in the line.
 * @param y_a y coordinate of first point in the line.
 * @param y_b y coordinate of second point in the line.
 * @param value x coordinate to interpolate the y value for.
 * @return y value for interpolation at x=`value` of line drawn between (`x_a`, `y_a`) and (`x_b`,
 * `y_b`).
 */
template <typename TXIntType, typename TYIntType>
constexpr TYIntType LinearInterpolate(TXIntType x_a, TXIntType x_b, TYIntType y_a, TYIntType y_b,
                                      TXIntType value) {
  if (x_b == x_a) {
    return y_a;
  }
  double percent = (static_cast<double>(value) - static_cast<double>(x_a)) /
                   (static_cast<double>(x_b) - static_cast<double>(x_a));
  return static_cast<TYIntType>(std::round(percent * (y_b - y_a))) + y_a;
}

/**
 * bytes_format structs allow you to quickly define new formats for BytesToString().
 * For example, say want a octal output, or you want a separator between characters,
 * then all you have to do is define a new struct and set kCharFormat appropriately.
 * Some commonly used styles are included in the print_style namespace;
 * others can be defined externally.
 */
namespace bytes_format {

// \x64\x65\xE9\x01
struct Hex {
  static inline constexpr std::string_view kCharFormat = "\\x%02X";
  static inline constexpr int kSizePerByte = 4;
  static inline constexpr bool kKeepPrintableChars = false;
};

// hi\xE0\x01
struct HexAsciiMix {
  static inline constexpr std::string_view kCharFormat = "\\x%02X";
  static inline constexpr int kSizePerByte = 4;
  static inline constexpr bool kKeepPrintableChars = true;
};

// 6465E901
struct HexCompact {
  static inline constexpr std::string_view kCharFormat = "%02X";
  static inline constexpr int kSizePerByte = 2;
  static inline constexpr bool kKeepPrintableChars = false;
};

// \b11001000\b11001011\b11010010\b0000001
struct Bin {
  // No kCharFormat, because we use template specialization for this case.
  static inline constexpr int kSizePerByte = 10;
  static inline constexpr bool kKeepPrintableChars = false;
};

}  // namespace bytes_format

template <typename TPrintStyle>
inline std::string BytesToString(std::string_view buf) {
  std::string res;

  res.reserve(buf.size() * TPrintStyle::kSizePerByte);

  for (char c : buf) {
    if (TPrintStyle::kKeepPrintableChars && std::isprint(c)) {
      res.append(1, c);
    } else {
      res.append(absl::StrFormat(TPrintStyle::kCharFormat, c));
    }
  }

  if (TPrintStyle::kKeepPrintableChars) {
    res.shrink_to_fit();
  }

  return res;
}

template <>
inline std::string BytesToString<bytes_format::Bin>(std::string_view buf) {
  static_assert(!bytes_format::Bin::kKeepPrintableChars, "Not implemented");

  std::string res;

  res.reserve(buf.size() * bytes_format::Bin::kSizePerByte);

  for (char c : buf) {
    res.append("\\b");
    res.append(std::bitset<8>(c).to_string());
  }

  return res;
}

/**
 * Converts an input hex sequence in ASCII to bytes.
 *
 * Input type must be well-formed hex representation, with optional separator.
 *
 * Note that this function is not optimized, and is not meant for use performance critical code.
 *
 * Examples:
 *  "0a2435383161353534662d"
 *  "0a 24 35 38 31 61 35 35 34 66 2d"
 *  "0a_24_35_38_31_61_35_35_34_66_2d"
 *  "0a:24:35:38:31:61:35:35:34:66:2d"
 *  "0a24353831 61353534"
 *
 * @tparam T Output container. Officially supported types are std::string and std::vector<uint8_t>.
 *           Presumably u8string will work too when we move to C++20.
 * @param hex Input string as ascii_hex.
 * @return Error or input string converted to sequence of bytes.
 */
template <class T>
inline StatusOr<T> AsciiHexToBytes(std::string s, const std::vector<char>& separators = {}) {
  for (auto& separator : separators) {
    s.erase(std::remove(std::begin(s), std::end(s), separator), std::end(s));
  }

  T bytes;

  for (unsigned int i = 0; i < s.length(); i += 2) {
    std::string byte_string = s.substr(i, 2);

    errno = 0;
    char* end_ptr;
    const char* byte_string_ptr = byte_string.c_str();
    uint8_t byte = static_cast<uint8_t>(strtol(byte_string_ptr, &end_ptr, 16));

    // Make sure we processed two ASCII characters, and there were no errors.
    if (end_ptr != byte_string_ptr + 2 || errno != 0) {
      return error::Internal("Could not parse value [position = $0]", i);
    }
    bytes.push_back(byte);
  }

  return bytes;
}

/**
 * @brief Returns a string_view for a different character type from the input C-style string.
 */
template <typename CharType, typename InCharType = char, size_t N>
std::basic_string_view<CharType> CreateStringView(const InCharType (&arr)[N]) {
  return std::basic_string_view<CharType>(reinterpret_cast<const CharType*>(arr), N - 1);
}

/**
 * @brief Returns a string_view for a different character type from the input C-style array.
 * Note the difference with the version above, which is for string literals, not arrays.
 */
template <typename CharType, typename InCharType = char, size_t N>
std::basic_string_view<CharType> CreateCharArrayView(const InCharType (&arr)[N]) {
  return std::basic_string_view<CharType>(reinterpret_cast<const CharType*>(arr), N);
}

/**
 * @brief Returns a string_view for a different character type from the input type.
 * Useful to convert basic_string_view<char> to basic_string_view<uint8_t> and vice versa.
 */
template <typename CharType = char, typename TContainer>
std::basic_string_view<CharType> CreateStringView(const TContainer& s) {
  return std::basic_string_view<CharType>(reinterpret_cast<const CharType*>(s.data()),
                                          s.size() * sizeof(typename TContainer::value_type));
}

/**
 * @brief Case-insensitive string comparison.
 */
struct CaseInsensitiveLess {
  struct NoCaseCompare {
    bool operator()(const unsigned char c1, const unsigned char c2) const {
      return std::tolower(c1) < std::tolower(c2);
    }
  };

  template <typename TStringType>
  bool operator()(const TStringType& s1, const TStringType& s2) const {
    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                        NoCaseCompare());
  }
};

/**
 * @brief creates an std::array based on passed in arguments.
 *
 * All arguments should be of the same time, or you get a compile time error.
 *
 * Usage:
 *  constexpr auto arr = MakeArray(1, 2, 3, 4);
 *     ---> arr = std::array<int, 4>(1, 2, 3, 4);
 */
template <typename... T>
constexpr auto MakeArray(T&&... values)
    -> std::array<typename std::decay<typename std::common_type<T...>::type>::type, sizeof...(T)> {
  return std::array<typename std::decay<typename std::common_type<T...>::type>::type, sizeof...(T)>{
      std::forward<T>(values)...};
}

namespace internal {
template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> MakeArrayImpl(T (&a)[N], std::index_sequence<I...>) {
  return {{a[I]...}};
}
}  // namespace internal

/**
 * @brief creates an std::array based on passed in C-style array.
 *
 * This variant of MakeArray is useful in cases where an array of complex types
 * is to be generated in an inline way.
 *
 * Example:
 *
 * struct Foo {
 *  int a;
 *  int b;
 * };
 *
 * // With other MakeArray variant, you can do the following:
 * constexpr auto kArray = MakeArray(Foo{1, 1}, Foo{2, 2});
 *
 * // With this variant you can instead use the form below,
 * // which eliminates repetitive type declaration,
 * // Note, however, the extra set of braces to declare the C-style array.
 * constexpr auto kArray = MakeArray<Foo>({{1, 1}, {2, 2}});
 */
// TODO(oazizi): Use std::to_array instead, once that is supported in implementations of C++20.
template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> MakeArray(const T (&a)[N]) {
  return internal::MakeArrayImpl(a, std::make_index_sequence<N>{});
}

namespace internal {
template <typename T, typename F, std::size_t... I>
constexpr auto ArrayTransformHelper(const std::array<T, sizeof...(I)>& arr, F&& f,
                                    std::index_sequence<I...>) {
  return MakeArray(f(arr[I])...);
}

}  // namespace internal

/**
 * Transforms passed in array by applying the function over each element and returning a new array.
 *
 * Usage:
 *   constexpr auto arr = MakeArray(1, 2, 3, 4);
 *   constexpr auto arr2 = ArrayTransform(arr, [](int x) { return x + 1; });
 *
 * @param arr The array.
 * @param f The function to apply.
 * @return returns new array that has been transformed using the function specified.
 */
template <typename T, typename F, std::size_t N>
constexpr auto ArrayTransform(const std::array<T, N>& arr, F&& f) {
  return internal::ArrayTransformHelper(arr, f, std::make_index_sequence<N>{});
}

template <typename T, typename F, std::size_t N = 0>
constexpr auto ArrayTransform(const std::array<T, 0>&, F&&) {
  return std::array<typename std::result_of_t<F&(T)>, 0>{};
}

// Attempts to cast raw value into an enum, and returns error if the value is not valid.
// Meant for use with PX_ASSIGN_OR_RETURN, Otherwise one should use magic_enum::enum_cast directly.
template <typename TEnum, typename TIn>
StatusOr<TEnum> EnumCast(TIn x) {
  auto enum_cast_var = magic_enum::enum_cast<TEnum>(x);
  if (!enum_cast_var.has_value()) {
    return error::Internal("Could not cast $0 as type $1", x, typeid(TEnum).name());
  }
  return enum_cast_var.value();
}

/**
 * Returns lines split from the input content.
 */
inline std::vector<std::string_view> GetLines(std::string_view content) {
  return absl::StrSplit(content, "\n", absl::SkipWhitespace());
}

/**
 * Automatically generate an operator<< for any class that defines ToString().
 * Function signature must be:
 *    std::string ToString() const;
 */
// TToString ensures this operator only applies to functions that have ToString() defined.
template <typename T, typename TToString = decltype(std::declval<T&>().ToString())>
inline std::ostream& operator<<(std::ostream& os, const T& v) {
  os << v.ToString();
  return os;
}

/**
 * Returns the value of the key that is the largest key not greater than the input key.
 * The "Floor" is used as analogy to the math operation.
 */
template <typename TContainerType>
typename TContainerType::const_iterator Floor(const TContainerType& c,
                                              const typename TContainerType::key_type& key) {
  auto iter = c.upper_bound(key);
  if (iter != c.begin()) {
    return --iter;
  }
  return c.end();
}

/**
 * Helper templates for std::visit. Will not be needed after C++20.
 * See: https://en.cppreference.com/w/cpp/utility/variant/visit
 */
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace px
