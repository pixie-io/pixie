#ifdef __linux__

#include "src/stirling/utils/fs_wrapper.h"

namespace pl {
namespace stirling {
namespace utils {

namespace fs = std::experimental::filesystem;

Status CreateSymlink(fs::path target, fs::path link) {
  std::error_code ec;
  fs::create_symlink(target, link, ec);
  if (ec) {
    return error::Internal("Failed to create symlink $0 -> $1. Message: $2", link.string(),
                           target.string(), ec.message());
  }
  return Status::OK();
}

Status CreateDirectories(fs::path dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    return error::Internal("Failed to create directory $0. Message: $0", dir.string(),
                           ec.message());
  }
  return Status::OK();
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl

#endif
