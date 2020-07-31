#pragma once

#include <string>

#include <gmock/gmock.h>

#include "src/common/system/config.h"

namespace pl {
namespace system {

class MockConfig : public Config {
 public:
  MOCK_CONST_METHOD0(HasConfig, bool());
  MOCK_CONST_METHOD0(PageSize, int64_t());
  MOCK_CONST_METHOD0(KernelTicksPerSecond, int64_t());
  MOCK_CONST_METHOD0(ClockRealTimeOffset, uint64_t());
  MOCK_CONST_METHOD0(sysfs_path, const std::filesystem::path&());
  MOCK_CONST_METHOD0(host_path, const std::filesystem::path&());
  MOCK_CONST_METHOD0(proc_path, const std::filesystem::path&());
};

}  // namespace system
}  // namespace pl
