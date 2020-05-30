#pragma once

#include <map>
#include <string>

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

  std::string DebugString() const {
    return absl::Substitute("[name='$0' value='$1']", name, value);
  }
};

// This allows GoogleTest to print NV values.
inline std::ostream& operator<<(std::ostream& os, const NV& nv) {
  os << nv.DebugString();
  return os;
}

}  // namespace stirling
}  // namespace pl
