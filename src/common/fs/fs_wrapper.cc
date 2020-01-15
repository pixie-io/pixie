#ifdef __linux__

#include <system_error>

#include "src/common/fs/fs_wrapper.h"

namespace pl {
namespace fs {

Status CreateSymlink(std::filesystem::path target, std::filesystem::path link) {
  std::error_code ec;
  std::filesystem::create_symlink(target, link, ec);
  if (ec) {
    return error::Internal("Failed to create symlink $0 -> $1. Message: $2", link.string(),
                           target.string(), ec.message());
  }
  return Status::OK();
}

Status CreateDirectories(std::filesystem::path dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return error::Internal("Failed to create directory $0. Message: $0", dir.string(),
                           ec.message());
  }
  return Status::OK();
}

pl::StatusOr<std::filesystem::path> ReadSymlink(std::filesystem::path symlink) {
  std::error_code ec;
  std::filesystem::path res = std::filesystem::read_symlink(symlink, ec);
  if (ec) {
    return pl::error::Internal("Could not read symlink: $0", symlink.string());
  }
  return res;
}

}  // namespace fs
}  // namespace pl

#endif
