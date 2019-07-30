#pragma once

#include <gmock/gmock.h>

#include "src/common/system/config.h"

namespace pl {
namespace system {

class MockConfig : public Config {
 public:
  MOCK_CONST_METHOD0(HasConfig, bool());
  MOCK_CONST_METHOD0(PageSize, int());
  MOCK_CONST_METHOD0(KernelTicksPerSecond, int());
  MOCK_CONST_METHOD0(ClockRealTimeOffset, uint64_t());
  MOCK_CONST_METHOD0(sysfs_path, std::string_view());
  MOCK_CONST_METHOD0(proc_path, std::string_view());
};

}  // namespace system
}  // namespace pl
