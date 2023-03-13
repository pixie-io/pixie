/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <system_error>

#include "src/common/fs/fs_wrapper.h"

namespace px {
namespace fs {

std::filesystem::path TempDirectoryPath() {
  std::error_code ec;
  std::filesystem::path tmp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    LOG(WARNING) << absl::Substitute(
        "Could not find temp directory from OS. Using /tmp instead. Message: $0", ec.message());
    tmp_dir = "/tmp";
  }
  return tmp_dir;
}

Status CreateSymlink(const std::filesystem::path& target, const std::filesystem::path& link) {
  std::error_code ec;
  std::filesystem::create_symlink(target, link, ec);
  if (ec) {
    if (ec.value() == EEXIST) {
      return error::AlreadyExists(
          "Failed to create symlink $0 -> $1. The link already exists. "
          "Message: $2",
          link.string(), target.string(), ec.message());
    }
    return error::System("Failed to create symlink $0 -> $1. Message: $2", link.string(),
                         target.string(), ec.message());
  }
  return Status::OK();
}

Status CreateDirectories(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return error::System("Failed to create directory $0. Message: $1", dir.string(), ec.message());
  }
  return Status::OK();
}

px::StatusOr<std::filesystem::path> ReadSymlink(const std::filesystem::path& symlink) {
  std::error_code ec;
  std::filesystem::path res = std::filesystem::read_symlink(symlink, ec);
  if (ec) {
    return error::System("Could not read symlink: $0. Message: $1", symlink.string(), ec.message());
  }
  return res;
}

std::filesystem::path JoinPath(const std::vector<const std::filesystem::path*>& paths) {
  std::filesystem::path res;
  for (const auto& p : paths) {
    if (p->empty()) {
      continue;
    }
    if (res.empty()) {
      res = *p;
    } else {
      res /= p->relative_path();
    }
  }
  return res;
}

Status CreateSymlinkIfNotExists(const std::filesystem::path& target,
                                const std::filesystem::path& link) {
  PX_RETURN_IF_ERROR(fs::CreateDirectories(link.parent_path()));

  // Attempt to create the symlink, but ignore the return status.
  // Why? Because if multiple instances are running in parallel, this CreateSymlink could fail.
  // That's okay. The real check to make sure the link is created is below.
  Status s = fs::CreateSymlink(target, link);
  PX_UNUSED(s);

  PX_ASSIGN_OR_RETURN(std::filesystem::path actual_target, fs::ReadSymlink(link));
  if (target != actual_target) {
    return error::Internal("Symlink not as expected [desired=$0, actual=$1]", target.c_str(),
                           actual_target.c_str());
  }
  return Status::OK();
}

bool Exists(const std::filesystem::path& path) {
  std::error_code ec;
  bool exists = std::filesystem::exists(path, ec);
  if (ec) {
    // This is very unlikely, so we just log an error and return false;
    LOG(DFATAL) << absl::Substitute("OS API error on path $0 [ec=$1]", path.string(), ec.message());
    return false;
  }
  return exists;
}

#define WRAP_BOOL_FN(expr) \
  std::error_code ec;      \
  if (expr) {              \
    return Status::OK();   \
  }

Status Copy(const std::filesystem::path& from, const std::filesystem::path& to,
            std::filesystem::copy_options options) {
  WRAP_BOOL_FN(std::filesystem::copy_file(from, to, options, ec));
  return error::InvalidArgument("Could not copy from $0 to $1 [ec=$2]", from.string(), to.string(),
                                ec.message());
}

Status Remove(const std::filesystem::path& path) {
  WRAP_BOOL_FN(std::filesystem::remove(path, ec));
  return error::InvalidArgument("Could not delete $0 [ec=$1]", path.string(), ec.message());
}

Status RemoveAll(const std::filesystem::path& path) {
  std::error_code ec;
  // Apparently, remove_all() uses -1 but in an unsigned type to indicate an error.
  // https://en.cppreference.com/w/cpp/filesystem/remove
  constexpr std::uintmax_t kErrorCode = static_cast<std::uintmax_t>(-1);
  constexpr std::uintmax_t kNothingRemoved = static_cast<std::uintmax_t>(0);
  const std::uintmax_t r = std::filesystem::remove_all(path, ec);
  if (r == kErrorCode) {
    return error::InvalidArgument("Could not delete $0 [ec=$1]", path.string(), ec.message());
  }
  if (r == kNothingRemoved) {
    return error::InvalidArgument("No such path $0 [ec=$1]", path.string(), ec.message());
  }
  return Status::OK();
}

Status Chown(const std::filesystem::path& path, const uid_t uid, const gid_t gid) {
  const int r = chown(path.string().c_str(), uid, gid);
  if (r != 0) {
    char const* const msg = "Could not chown $0 to uid: $1, gid: $2. $3 ($4).";
    return error::InvalidArgument(msg, path.string(), uid, gid, strerror(errno), errno);
  }
  return Status::OK();
}

StatusOr<struct stat> Stat(const std::filesystem::path& path) {
  struct stat sb;
  const int r = stat(path.string().c_str(), &sb);
  if (r != 0) {
    char const* const msg = "Could not stat $0. $1 ($2).";
    return error::InvalidArgument(msg, path.string(), strerror(errno), errno);
  }
  return sb;
}

StatusOr<int64_t> SpaceAvailableInBytes(const std::filesystem::path& path) {
  std::error_code ec;
  const std::filesystem::space_info si = std::filesystem::space(path, ec);
  if (ec.value()) {
    return error::InvalidArgument("Could not check space available $0 [ec=$1]", path.string(),
                                  ec.message());
  }
  return si.available;
}

StatusOr<bool> IsEmpty(const std::filesystem::path& f) {
  std::error_code ec;
  bool val = std::filesystem::is_empty(f, ec);
  if (ec.value()) {
    return error::InvalidArgument("Could not check for emptiness $0 [ec=$1]", f.string(),
                                  ec.message());
  }
  return val;
}

StatusOr<std::filesystem::path> Absolute(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::path abs_path = std::filesystem::absolute(path, ec);
  if (ec) {
    return error::System(ec.message());
  }
  return abs_path;
}

StatusOr<std::filesystem::path> Canonical(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::path canonical_path = std::filesystem::canonical(path, ec);
  if (ec) {
    return error::System(ec.message());
  }
  return canonical_path;
}

StatusOr<std::filesystem::path> Relative(const std::filesystem::path& path,
                                         const std::filesystem::path& base) {
  std::error_code ec;
  auto res = std::filesystem::relative(path, base, ec);
  if (ec) {
    return error::System(ec.message());
  }
  return res;
}

StatusOr<bool> Equivalent(const std::filesystem::path& p1, const std::filesystem::path& p2) {
  std::error_code ec;
  auto res = std::filesystem::equivalent(p1, p2, ec);
  if (ec) {
    return error::System(ec.message());
  }
  return res;
}

namespace {

bool IsParent(const std::filesystem::path& child, const std::filesystem::path& parent) {
  auto c_iter = child.begin();
  auto p_iter = parent.begin();
  for (; c_iter != child.end() && p_iter != parent.end(); ++c_iter, ++p_iter) {
    if (*c_iter != *p_iter) {
      break;
    }
  }
  return p_iter == parent.end();
}

}  // namespace

StatusOr<std::filesystem::path> GetChildRelPath(std::filesystem::path child,
                                                std::filesystem::path parent) {
  if (child.empty() || parent.empty()) {
    return error::InvalidArgument("Both paths must not be empty, child=$0, parent=$1",
                                  child.string(), parent.string());
  }
  // Relative() returns ".." when child is a sibling of parent. IsParent() rules out such cases.
  if (!IsParent(child, parent)) {
    return error::InvalidArgument("Path=$0 is not parent of child=$1", parent.string(),
                                  child.string());
  }
  PX_ASSIGN_OR_RETURN(std::filesystem::path res, Relative(child, parent));
  // Relative() returns "." when child and parent are the same. "." complicates the path joining.
  if (res == ".") {
    res.clear();
  }
  return res;
}

std::vector<PathSplit> EnumerateParentPaths(const std::filesystem::path& path) {
  std::vector<PathSplit> res;

  std::filesystem::path child;
  std::filesystem::path parent = path;
  while (parent != parent.parent_path()) {
    res.push_back(PathSplit{parent, child});
    if (child.empty()) {
      child = parent.filename();
    } else {
      child = parent.filename() / child;
    }
    parent = parent.parent_path();
  }
  if (path.is_absolute()) {
    res.push_back(PathSplit{"/", path.relative_path()});
  }
  return res;
}

}  // namespace fs
}  // namespace px
