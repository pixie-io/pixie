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
    return error::Internal("Failed to create directory $0. Message: $1", dir.string(),
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

std::filesystem::path JoinPath(const std::vector<const std::filesystem::path*>& paths) {
  std::filesystem::path res;
  for (const auto& p : paths) {
    if (!res.empty() && p->is_relative()) {
      res += std::filesystem::path::preferred_separator;
    }
    res += *p;
  }
  return res;
}

Status CreateSymlinkIfNotExists(std::filesystem::path target, std::filesystem::path link) {
  PL_RETURN_IF_ERROR(fs::CreateDirectories(link.parent_path()));

  // Attempt to create the symlink, but ignore the return status.
  // Why? Because if multiple instances are running in parallel, this CreateSymlink could fail.
  // That's okay. The real check to make sure the link is created is below.
  Status s = fs::CreateSymlink(target, link);
  PL_UNUSED(s);

  PL_ASSIGN_OR_RETURN(std::filesystem::path actual_target, fs::ReadSymlink(link));
  if (target != actual_target) {
    return error::Internal("Symlink not as expected [desired=$0, actual=$1]", target.c_str(),
                           actual_target.c_str());
  }
  return Status::OK();
}

}  // namespace fs
}  // namespace pl

#endif
