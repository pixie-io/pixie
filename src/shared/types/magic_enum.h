#pragma once
#include <magic_enum.hpp>

#include "src/shared/types/types.h"

namespace magic_enum {
template <>
struct enum_range<pl::types::SemanticType> {
  static constexpr int min = 0;
  static constexpr int max = 1000;
};
}  // namespace magic_enum
