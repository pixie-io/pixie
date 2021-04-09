#pragma once

#include <magic_enum.hpp>

#include "src/shared/types/typespb/types.pb.h"

namespace magic_enum::customize {
template <>
struct enum_range<pl::types::SemanticType> {
  static constexpr int min = 0;
  static constexpr int max = 2000;
};
}  // namespace magic_enum::customize
