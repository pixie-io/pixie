#include "src/common/system/system_info.h"

#include <string>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"

namespace pl {
namespace system {

namespace {
void LogFileContents(std::filesystem::path file) {
  StatusOr<std::string> contents = ReadFileToString(file);
  if (contents.ok()) {
    LOG(INFO) << absl::Substitute("$0:\n$1", file.string(), contents.ConsumeValueOrDie());
  }
}
}  // namespace

void LogSystemInfo() {
  const system::Config& sysconfig = system::Config::GetInstance();
  LOG(INFO) << absl::StrCat("Location of proc: ", sysconfig.proc_path().string());
  LOG(INFO) << absl::StrCat("Location of sysfs: ", sysconfig.sysfs_path().string());

  // Log /proc/version.
  LogFileContents(sysconfig.proc_path() / "version");

  // Log /etc/*-release (e.g. /etc/lsb-release, /etc/os-release).
  std::filesystem::path etc_path("/etc");
  const std::filesystem::path& host_path = sysconfig.host_path();
  auto host_etc_path = fs::JoinPath({&host_path, &etc_path});
  for (auto& p : std::filesystem::directory_iterator(host_etc_path)) {
    if (absl::EndsWith(p.path().string(), "-release")) {
      LogFileContents(p);
    }
  }
}

}  // namespace system
}  // namespace pl
