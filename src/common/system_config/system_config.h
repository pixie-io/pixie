#pragma once

#include <memory>

#include "src/common/base/base.h"

namespace pl {
namespace common {

/**
 * This interface provides access to global system config.
 */
class SystemConfig : public NotCopyable {
 public:
  /**
   * Create an OS specific SystemConfig instance.
   * @return unique_ptr to SystemConfig.
   */
  static SystemConfig* GetInstance();

  virtual ~SystemConfig() {}

  /**
   * Checks if system config information is available.
   * @return true if system config is available
   */
  virtual bool HasSystemConfig() const = 0;

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

 protected:
  SystemConfig() {}
};

}  // namespace common
}  // namespace pl
