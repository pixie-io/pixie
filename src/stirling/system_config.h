#pragma once

#include <memory>

namespace pl {
namespace stirling {

/**
 * This interface provides access to global system config.
 */
class SystemConfig {
 public:
  /**
   * Create an OS specific SystemConfig instance.
   * @return unique_ptr to SystemConfig.
   */
  std::unique_ptr<SystemConfig> Create();

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
};

}  // namespace stirling
}  // namespace pl
