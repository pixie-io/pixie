#include "src/stirling/obj_tools/obj_tools.h"

namespace pl {
namespace stirling {
namespace obj_tools {

namespace fs = std::experimental::filesystem;

// NOTE: Today this function is not robust and looks for a very specific pattern.
// IT IS NOT PRODUCTION READY.
pl::StatusOr<fs::path> ResolveExe(fs::path proc_pid) {
  fs::path exe = proc_pid / "exe";
  fs::path mounts = proc_pid / "mounts";

  std::error_code ec;
  exe = fs::read_symlink(exe, ec);
  if (ec) {
    return pl::error::Internal("Could not find exe symlink for $0", proc_pid.string());
  }

  std::string original_exe = exe.string();

  pl::StatusOr<std::string> s = pl::ReadFileToString(mounts);
  if (!s.ok()) {
    return exe;
  }
  std::string mounts_content = s.ConsumeValueOrDie();

  std::vector<std::string_view> lines = absl::StrSplit(mounts_content, '\n');

  if (lines.empty()) {
    return exe;
  }

  const auto& line = lines[0];

  if (line.empty()) {
    return exe;
  }

  std::vector<std::string_view> mount_split = absl::StrSplit(line, ' ');

  CHECK(mount_split.size() >= 4);

  std::string_view path = mount_split[1];
  std::string_view type = mount_split[2];
  std::string_view attrs = mount_split[3];

  if (path != "/" || type != "overlay") {
    return exe;
  }

  std::vector<std::string_view> attrs_vec = absl::StrSplit(attrs, ',');

  constexpr std::string_view kUpperDir = "upperdir=";
  constexpr std::string_view kDiffSuffix = "/diff";
  bool transformed = false;
  for (const auto& attr : attrs_vec) {
    auto pos = attr.find(kUpperDir);
    if (pos != std::string_view::npos) {
      std::string_view s = attr.substr(pos + kUpperDir.size());
      DCHECK(absl::EndsWith(s, kDiffSuffix));
      s.remove_suffix(kDiffSuffix.size());
      exe = fs::path(absl::StrCat(s, "/merged/", exe.string()));
      LOG(INFO) << absl::Substitute("New exe path: $0", exe.string());
      transformed = true;
      break;
    }
  }

  CHECK(transformed);

  LOG(INFO) << absl::Substitute("Resolved Binary: $0 -> $1", original_exe, exe.string());

  return exe;
}

std::map<std::string, std::vector<int>> GetActiveBinaries(fs::path proc) {
  std::map<std::string, std::vector<int>> binaries;

  for (const auto& p : fs::directory_iterator(proc)) {
    VLOG(1) << absl::Substitute("Directory: $0", p.path().string());
    int pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      continue;
    }

    pl::StatusOr<fs::path> s = ResolveExe(p);
    if (!s.ok()) {
      LOG(WARNING) << absl::Substitute("Ignoring $0: $1", p.path().string(), s.msg());
      continue;
    }
    fs::path exe = s.ConsumeValueOrDie();

    if (fs::exists(exe)) {
      std::error_code ec;
      fs::path canonical_exe = fs::canonical(exe, ec);
      if (ec) {
        LOG(WARNING) << absl::Substitute("Ignoring $0: Could not find canonical path.",
                                         exe.string());
        continue;
      }
      binaries[canonical_exe].push_back(pid);
    }
  }

  LOG(INFO) << "Number of unique binaries found: " << binaries.size();

  return binaries;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
