#include <fstream>
#include <string>

#include "src/common/base/file.h"
#include "src/common/base/logging.h"

namespace pl {
std::string FileContentsOrDie(const std::string& filename) {
  std::ifstream ifs(filename);
  if (!ifs) {
    LOG(FATAL) << "Failed to read file: " << filename;
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  ifs.close();
  return buffer.str();
}
}  // namespace pl
