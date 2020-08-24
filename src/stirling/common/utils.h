#pragma once

#include <map>
#include <string>

#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {

inline size_t CountStringMapSize(const std::multimap<std::string, std::string>& name_values) {
  size_t res = 0;
  for (const auto& [name, value] : name_values) {
    res += name.size();
    res += value.size();
  }
  return res;
}

struct NV {
  std::string name;
  std::string value;

  std::string ToString() const { return absl::Substitute("[name='$0' value='$1']", name, value); }
};

// This allows GoogleTest to print NV values.
DEFINE_TO_STRING_OUTPUT_OPERATOR(NV);

inline void RemoveRepeatingSuffix(std::string_view* str, char c) {
  size_t pos = str->find_last_not_of(c);
  if (pos != std::string_view::npos) {
    str->remove_suffix(str->size() - pos - 1);
  }
}

}  // namespace stirling
}  // namespace pl
