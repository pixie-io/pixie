#pragma once

// Only add the specialized enum_range if magic_enum.hpp has not been included yet, otherwise this
// leads to errors.
#ifndef NEARGYE_MAGIC_ENUM_HPP
#include <magic_enum.hpp>

#include "src/shared/types/types.h"

namespace magic_enum {
template <>
struct enum_range<pl::types::SemanticType> {
  static constexpr int min = 0;
  static constexpr int max = 1000;
};
}  // namespace magic_enum
#endif
