#pragma once

#include <memory>
#include <string>

#include "src/common/base/base.h"

namespace pl {
namespace system {

/**
 * This interface provides access to global system config.
 */
class Config : public NotCopyable {
 public:
  /**
   * Create an OS specific SystemConfig instance.
   * @return const reference to SystemConfig.
   */
  static const Config& GetInstance();

  virtual ~Config() {}

  /**
   * Checks if system config information is available.
   * @return true if system config is available
   */
  virtual bool HasConfig() const = 0;

  /**
   * Get the page size in the kernel.
   * @return page size in bytes.
   */
  virtual int PageSize() const = 0;

  /**
   * Get the Kernel ticks per second.
   * @return int kernel ticks per second.
   */
  virtual int KernelTicksPerSecond() const = 0;

  /**
   * @brief If recording nsecs in your bt file, this function can be used to find the offset for
   * convert the result into realtime.
   */
  virtual uint64_t ClockRealTimeOffset() const = 0;

  /**
   * Get the sysfs path.
   * @return string the sysfs path.
   */
  virtual std::string_view sysfs_path() const = 0;

  /**
   * Get the host root path.
   * @return string the host root path.
   */
  virtual std::string_view host_path() const = 0;

  /**
   * Get the proc path.
   * @return string the proc path.
   */
  virtual std::string proc_path() const = 0;

 protected:
  Config() {}
};

}  // namespace system
}  // namespace pl
