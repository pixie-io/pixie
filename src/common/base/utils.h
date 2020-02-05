#pragma once

#include <unistd.h>

#include <array>
#include <bitset>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include "src/common/base/error.h"
#include "src/common/base/statusor.h"

namespace pl {

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

enum class Radix {
  kBin,
  kHex,
};

// Policy for converting a printable char.
enum class PrintConvPolicy {
  kKeep,
  kToDigit,
};

inline std::string Repr(std::string_view buf, Radix radix = Radix::kHex,
                        PrintConvPolicy policy = PrintConvPolicy::kKeep) {
  std::string res;
  for (char c : buf) {
    if (std::isprint(c) && policy == PrintConvPolicy::kKeep) {
      res.append(1, c);
    } else {
      switch (radix) {
        case Radix::kBin:
          res.append("\\b");
          res.append(std::bitset<8>(c).to_string());
          break;
        case Radix::kHex:
          res.append(absl::StrFormat("\\x%02X", c));
          break;
      }
    }
  }
  return res;
}

inline std::string BytesToAsciiHex(std::string_view buf) {
  return Repr(buf, Radix::kHex, PrintConvPolicy::kToDigit);
}

inline std::string BytesToAsciiBin(std::string_view buf) {
  return Repr(buf, Radix::kBin, PrintConvPolicy::kToDigit);
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
template <typename CharType, size_t N>
std::basic_string_view<CharType> CreateStringView(const char (&arr)[N]) {
  return std::basic_string_view<CharType>(reinterpret_cast<const CharType*>(arr), N - 1);
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

}  // namespace pl
