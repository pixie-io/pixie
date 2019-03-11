#pragma once

#include <cstdint>
#include "src/farmhash.h"

#include "src/shared/types/types.h"

namespace pl {
namespace types {
namespace utils {
/**
 * Combines two 64-bit hash values (borrowed from CityHash).
 * @param h1 The first hash.
 * @param h2 The second hash.
 * Both hashes can be from the same family.
 * @return A 64-bit combined hash.
 */
static uint64_t HashCombine(uint64_t h1, uint64_t h2) {
  // Murmur-inspired hashing.
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t a = (h1 ^ h2) * kMul;
  a ^= (a >> 47);
  uint64_t b = (h2 ^ a) * kMul;
  b ^= (b >> 47);
  b *= kMul;
  return b;
}

// PL_CARNOT_UPDATE_FOR_NEW_TYPES
template <typename T>
struct hash {};

template <>
struct hash<BoolValue> {
  uint64_t operator()(BoolValue val) { return static_cast<uint64_t>(val.val); }
};

template <>
struct hash<Int64Value> {
  uint64_t operator()(Int64Value val) {
    return ::util::Hash64(reinterpret_cast<const char *>(&(val.val)), sizeof(int64_t));
  }
};

template <>
struct hash<Float64Value> {
  uint64_t operator()(Float64Value val) {
    return ::util::Hash64(reinterpret_cast<const char *>(&(val.val)), sizeof(double));
  }
};

template <>
struct hash<StringValue> {
  uint64_t operator()(StringValue val) { return ::util::Hash64(val); }
};

template <>
struct hash<Time64NSValue> {
  uint64_t operator()(Time64NSValue val) {
    return ::util::Hash64(reinterpret_cast<const char *>(&(val)), sizeof(Time64NSValue));
  }
};

}  // namespace utils
}  // namespace types
}  // namespace pl
