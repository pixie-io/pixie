#include "src/common/system/config.h"

#include <unistd.h>

#include "src/common/base/base.h"
#include "src/common/system/system.h"

namespace pl {
namespace system {

DEFINE_string(sysfs_path, gflags::StringFromEnv("PL_SYSFS_PATH", "/sys/fs"),
              "The path to the sysfs directory.");

DEFINE_string(host_path, gflags::StringFromEnv("PL_HOST_PATH", ""),
              "The path to the host root directory.");

#ifdef __linux__
#include <ctime>

class ConfigImpl final : public Config {
 public:
  ConfigImpl() { InitClockRealTimeOffset(); }

  bool HasConfig() const override { return true; }

  int PageSize() const override { return sysconf(_SC_PAGESIZE); }

  int KernelTicksPerSecond() const override { return sysconf(_SC_CLK_TCK); }

  uint64_t ClockRealTimeOffset() const override { return real_time_offset_; }

  std::string_view sysfs_path() const override { return FLAGS_sysfs_path; }

  std::string_view host_path() const override { return FLAGS_host_path; }

  std::string proc_path() const override { return absl::StrCat(FLAGS_host_path, "/proc"); }

 private:
  uint64_t real_time_offset_ = 0;

  // Utility function to convert time as recorded by in monotonic clock (aka steady_clock)
  // to real time (aka system_clock).
  // TODO(oazizi): if machine is ever suspended, this Init would have to be called again.
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

  int PageSize() const override { LOG(FATAL) << "PageSize() is not implemented on this OS."; }

  int KernelTicksPerSecond() const override {
    LOG(FATAL) << "KernelTicksPerSecond() is not implemented on this OS.";
  }

  uint64_t ClockRealTimeOffset() const override {
    LOG(FATAL) << "ClockRealTimeOffset() is not implemented on this OS.";
  }

  std::string proc_path() const override {
    LOG(FATAL) << "proc_path() is not implemented on this OS.";
  }

  std::string_view host_path() const override {
    LOG(FATAL) << "host_path() is not implemented on this OS.";
  }

  std::string_view sysfs_path() const override {
    LOG(FATAL) << "sysfs_path() is not implemented on this OS.";
  }
};

#endif

const Config& Config::GetInstance() {
  static ConfigImpl instance;
  return instance;
}

}  // namespace system
}  // namespace pl
