#include <unistd.h>

#include "src/common/base/base.h"
#include "src/common/system_config/system_config.h"

namespace pl {
namespace common {

#ifdef __linux__

class SystemConfigImpl final : public SystemConfig {
 public:
  bool HasSystemConfig() const override { return true; }

  int PageSize() const override { return sysconf(_SC_PAGESIZE); }

  int KernelTicksPerSecond() const override { return sysconf(_SC_CLK_TCK); }
};

#else

class SystemConfigImpl final : public SystemConfig {
 public:
  bool HasSystemConfig() const override { return false; }

  int PageSize() const override { LOG(FATAL) << "PageSize() is not implemented on this OS."; }

  int KernelTicksPerSecond() const override {
    LOG(FATAL) << "KernelTicksPerSecond() is not implemented on this OS.";
  }
};

#endif

std::unique_ptr<SystemConfig> SystemConfig::Create() {
  return std::make_unique<SystemConfigImpl>();
}

}  // namespace common
}  // namespace pl
