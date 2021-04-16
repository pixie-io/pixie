#include "src/common/exec/exec.h"

#include <memory>

#include "src/common/base/error.h"

namespace px {

StatusOr<std::string> Exec(std::string cmd) {
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (pipe == nullptr) {
    return error::Internal("popen() failed!");
  }

  std::array<char, 128> buffer;
  std::string result;
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

}  // namespace px
