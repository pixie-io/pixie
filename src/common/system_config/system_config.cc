#include <unistd.h>

#include "src/common/base/base.h"
#include "src/common/system_config/system_config.h"

namespace pl {
namespace common {

DEFINE_string(sysfs_path, gflags::StringFromEnv("PL_SYSFS_PATH", "/sys/fs"),
              "The path to the sysfs directory.");

DEFINE_string(proc_path, gflags::StringFromEnv("PL_PROC_PATH", "/proc"),
              "The path to the proc directory.");

#ifdef __linux__
#include <cstring>
#include <ctime>

class SystemConfigImpl final : public SystemConfig {
 public:
  SystemConfigImpl() { InitClockRealTimeOffset(); }

  bool HasSystemConfig() const override { return true; }

  int PageSize() const override { return sysconf(_SC_PAGESIZE); }

  int KernelTicksPerSecond() const override { return sysconf(_SC_CLK_TCK); }

  uint64_t ClockRealTimeOffset() const override { return real_time_offset_; }

  std::string_view sysfs_path() const override { return FLAGS_sysfs_path; }

  std::string_view proc_path() const override { return FLAGS_proc_path; }

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

class SystemConfigImpl final : public SystemConfig {
 public:
  bool HasSystemConfig() const override { return false; }

  int PageSize() const override { LOG(FATAL) << "PageSize() is not implemented on this OS."; }

  int KernelTicksPerSecond() const override {
    LOG(FATAL) << "KernelTicksPerSecond() is not implemented on this OS.";
  }

  uint64_t ClockRealTimeOffset() const override {
    LOG(FATAL) << "ClockRealTimeOffset() is not implemented on this OS.";
  }

  std::string_view sysfs_path() const override { return FLAGS_sysfs_path; }

  std::string_view proc_path() const override { return FLAGS_proc_path; }
};

#endif

SystemConfig* SystemConfig::GetInstance() {
  static SystemConfigImpl instance;
  return &instance;
}

}  // namespace common
}  // namespace pl
