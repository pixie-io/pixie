#pragma once

#include <string>

#include <absl/strings/substitute.h>

namespace px {
namespace stirling {

// TODO(oazizi): Move to socket_tracer. It's the only user.
struct NV {
  std::string name;
  std::string value;

  std::string ToString() const { return absl::Substitute("[name='$0' value='$1']", name, value); }
};

inline void RemoveRepeatingSuffix(std::string_view* str, char c) {
  size_t pos = str->find_last_not_of(c);
  if (pos != std::string_view::npos) {
    str->remove_suffix(str->size() - pos - 1);
  }
}

}  // namespace stirling
}  // namespace px
