#include <sys/stat.h>

#include <fstream>
#include <string>

#include "src/common/base/error.h"
#include "src/common/base/file.h"
#include "src/common/base/logging.h"

namespace px {

std::string FileContentsOrDie(const std::string& filename) {
  return ReadFileToString(filename).ConsumeValueOrDie();
}

StatusOr<std::string> ReadFileToString(const std::string& filename, std::ios_base::openmode mode) {
  std::ifstream ifs(filename, mode);
  if (!ifs.good()) {
    return error::Internal("Failed to read file $0 ($1)", filename, strerror(errno));
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  return buffer.str();
}

Status WriteFileFromString(const std::string& filename, std::string_view contents,
                           std::ios_base::openmode mode) {
  std::ofstream ofs(filename, mode);
  if (!ofs.good()) {
    return error::Internal("Failed to write file $0", filename);
  }
  ofs << contents;
  return Status::OK();
}

}  // namespace px
