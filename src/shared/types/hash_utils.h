#pragma once

#include <farmhash.h>

#include <cstdint>

#include "src/shared/types/types.h"

namespace px {
namespace types {
namespace utils {

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
    return ::util::Hash64(reinterpret_cast<const char*>(&(val.val)), sizeof(int64_t));
  }
};

template <>
struct hash<Float64Value> {
  uint64_t operator()(Float64Value val) {
    return ::util::Hash64(reinterpret_cast<const char*>(&(val.val)), sizeof(double));
  }
};

template <>
struct hash<StringValue> {
  uint64_t operator()(StringValue val) { return ::util::Hash64(val); }
};

template <>
struct hash<Time64NSValue> {
  uint64_t operator()(Time64NSValue val) {
    return ::util::Hash64(reinterpret_cast<const char*>(&(val)), sizeof(Time64NSValue));
  }
};

}  // namespace utils
}  // namespace types
}  // namespace px
