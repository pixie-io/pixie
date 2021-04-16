#pragma once

#include <string>
#include <thread>

namespace px {

inline std::string CurrentThreadIDAsString() {
  std::stringstream ss;
  ss << std::this_thread::get_id();
  return ss.str();
}

}  // namespace px
