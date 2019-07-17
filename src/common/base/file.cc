#include <fstream>
#include <string>

#include "src/common/base/error.h"
#include "src/common/base/file.h"
#include "src/common/base/logging.h"

namespace pl {
std::string FileContentsOrDie(const std::string& filename) {
  return ReadFileToString(filename).ConsumeValueOrDie();
}

StatusOr<std::string> ReadFileToString(const std::string& filename) {
  std::ifstream ifs(filename);
  if (!ifs.good()) {
    return error::Internal("Failed to read file $0", filename);
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  return buffer.str();
}

Status WriteFileFromString(const std::string& filename, std::string_view contents) {
  std::ofstream ofs(filename);
  if (!ofs.good()) {
    return error::Internal("Failed to write file $0", filename);
  }
  ofs << contents;
  return Status::OK();
}

}  // namespace pl
