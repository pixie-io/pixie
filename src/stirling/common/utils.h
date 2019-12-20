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

}  // namespace stirling
}  // namespace pl
