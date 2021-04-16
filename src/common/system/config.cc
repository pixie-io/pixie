#include "src/common/system/config.h"

#include <unistd.h>

#include "src/common/base/base.h"
#include "src/common/fs/fs_wrapper.h"

namespace px {
namespace system {

DEFINE_string(sysfs_path, gflags::StringFromEnv("PL_SYSFS_PATH", "/sys/fs"),
              "The path to the sysfs directory.");

DEFINE_string(host_path, gflags::StringFromEnv("PL_HOST_PATH", ""),
              "The path to the host root directory.");

#ifdef __linux__
#include <ctime>

class ConfigImpl final : public Config {
 public:
  ConfigImpl()
      : host_path_(FLAGS_host_path),
        sysfs_path_(FLAGS_sysfs_path),
        proc_path_(absl::StrCat(FLAGS_host_path, "/proc")) {
    InitClockRealTimeOffset();
  }

  bool HasConfig() const override { return true; }

  int64_t PageSize() const override { return sysconf(_SC_PAGESIZE); }

  int64_t KernelTicksPerSecond() const override { return sysconf(_SC_CLK_TCK); }

  uint64_t ClockRealTimeOffset() const override { return real_time_offset_; }

  const std::filesystem::path& sysfs_path() const override { return sysfs_path_; }

  const std::filesystem::path& host_path() const override { return host_path_; }

  const std::filesystem::path& proc_path() const override { return proc_path_; }

  std::filesystem::path ToHostPath(const std::filesystem::path& p) const override {
    // If we're running in a container, convert path to be relative to our host mount.
    // Note that we mount host '/' to '/host' inside container.
    // Warning: must use JoinPath, because we are dealing with two absolute paths.
    return fs::JoinPath({&host_path_, &p});
  }

 private:
  uint64_t real_time_offset_ = 0;
  const std::filesystem::path host_path_;
  const std::filesystem::path sysfs_path_;
  const std::filesystem::path proc_path_;

  // Utility function to convert time as recorded by in monotonic clock (aka steady_clock)
  // to real time (aka system_clock).
  // TODO(oazizi): if machine is ever suspended, this function would have to be called again.
  void InitClockRealTimeOffset() {
    static constexpr uint64_t kSecToNanosecFactor = 1000000000;

    struct timespec time, real_time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    clock_gettime(CLOCK_REALTIME, &real_time);

    real_time_offset_ =
        kSecToNanosecFactor * (real_time.tv_sec - time.tv_sec) + real_time.tv_nsec - time.tv_nsec;
  }
};

#else

class ConfigImpl final : public Config {
 public:
  bool HasConfig() const override { return false; }

  int64_t PageSize() const override { LOG(FATAL) << "PageSize() is not implemented on this OS."; }

  int64_t KernelTicksPerSecond() const override {
    LOG(FATAL) << "KernelTicksPerSecond() is not implemented on this OS.";
  }

  uint64_t ClockRealTimeOffset() const override {
    LOG(FATAL) << "ClockRealTimeOffset() is not implemented on this OS.";
  }

  std::filesystem::path host_path() const override {
    LOG(FATAL) << "host_path() is not implemented on this OS.";
  }

  std::filesystem::path sysfs_path() const override {
    LOG(FATAL) << "sysfs_path() is not implemented on this OS.";
  }

  std::filesystem::path proc_path() const override {
    LOG(FATAL) << "proc_path() is not implemented on this OS.";
  }
};

#endif

namespace {
std::unique_ptr<ConfigImpl> g_instance;
}

const Config& Config::GetInstance() {
  if (g_instance == nullptr) {
    ResetInstance();
  }
  return *g_instance;
}

void Config::ResetInstance() { g_instance = std::make_unique<ConfigImpl>(); }

}  // namespace system
}  // namespace px
