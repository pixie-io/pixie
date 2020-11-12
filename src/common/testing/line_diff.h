#pragma once

#include <string>
#include <vector>

#include <absl/strings/substitute.h>

namespace pl {
namespace testing {

// Returns a text representation of the diffs between 2 sequences of text lines.
std::string Diff(const std::vector<std::string>& lhs, const std::vector<std::string>& rhs);

enum class DiffPolicy {
  // Diff all literal lines.
  kDefault,

  // Ignores blank lines.
  kIgnoreBlankLines,
};

// Returns a text representation of the line-based diffs of 2 text sequences.
std::string DiffLines(const std::string& lhs, const std::string& rhs,
                      DiffPolicy = DiffPolicy::kDefault);

}  // namespace testing
}  // namespace pl
