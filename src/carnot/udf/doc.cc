#include <functional>
#include <regex>
#include <string>

#include "src/carnot/udf/doc.h"

namespace pl {
namespace carnot {
namespace udf {

std::string DedentBlock(const std::string& in) {
  static std::regex dedenter{"\n.*\\|\\s"};
  auto s = std::regex_replace(std::string(in), dedenter, "\n");
  if (!s.empty() && s[0] == '\n') {
    s.erase(0, 1);
  }
  // Strip any trailing spaces.
  s.erase(std::find_if(s.rbegin(), s.rend(), std::bind1st(std::not_equal_to<char>(), ' ')).base(),
          s.end());

  return s;
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
