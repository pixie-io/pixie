#pragma once

#include <unistd.h>

#include <bitset>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

#include "absl/strings/str_format.h"

namespace pl {

inline bool IsRoot() { return (geteuid() == 0); }

/**
 * @brief Copy from BPF data pointed by a pointer. The target should be a memory aligned type to
 * necessitate this function. This is because BPF memory might lost memory alignment when copied
 * from BPF to perf buffer, in which case, a naive pointer cast might cause runtime error in ASAN,
 * or in rare situations where the compiler produces misaligned code on different CPU arches.
 */
template <typename MemAlignedType>
inline MemAlignedType CopyFromBPF(const void* data) {
  MemAlignedType result;
  memcpy(&result, data, sizeof(MemAlignedType));
  return result;
}

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

}  // namespace pl

// Provides a string view into a char array included in the binary via objcopy.
// Useful for include BPF programs that are copied into the binary.
#define OBJ_STRVIEW(varname, objname)     \
  extern char objname##_start;            \
  extern char objname##_end;              \
  inline const std::string_view varname = \
      std::string_view(&objname##_start, &objname##_end - &objname##_start);
