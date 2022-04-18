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

#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "src/common/base/base.h"
#include "src/common/clock/clock_conversion.h"

namespace px {
namespace system {

DECLARE_string(host_path);

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

  /**
   * Resets the underlying static instance. Used for testing purposes.
   * @param converter unique_ptr to a ClockConverter instance to use for the Config.
   */
  static void ResetInstance(std::unique_ptr<clock::ClockConverter> converter);
  static void ResetInstance();

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
  virtual int64_t PageSizeBytes() const = 0;

  /**
   * Get the Kernel ticks per second.
   * @return int kernel ticks per second.
   */
  virtual int64_t KernelTicksPerSecond() const = 0;

  /**
   * Get the Kernel tick time in nanoseconds.
   * @return int kernel ticks time.
   */
  virtual int64_t KernelTickTimeNS() const = 0;

  /**
   * If recording `nsecs` from bpf, this function can be used to
   * convert the result into realtime.
   */
  virtual uint64_t ConvertToRealTime(uint64_t monotonic_time) const = 0;

  /**
   * Get the sysfs path.
   */
  virtual const std::filesystem::path& sysfs_path() const = 0;

  /**
   * Get the host root path.
   */
  virtual const std::filesystem::path& host_path() const = 0;

  /**
   * Get the proc path.
   */
  virtual const std::filesystem::path& proc_path() const = 0;

  /**
   * Converts a path to host relative path, for when this binary is running inside a container.
   */
  virtual std::filesystem::path ToHostPath(const std::filesystem::path& p) const = 0;

  virtual clock::ClockConverter* clock_converter() const = 0;

 protected:
  Config() {}
};

}  // namespace system
}  // namespace px
