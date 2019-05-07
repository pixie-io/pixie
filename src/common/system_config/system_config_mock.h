#pragma once

#include <gmock/gmock.h>

#include "src/common/system_config/system_config.h"

namespace pl {
namespace common {

class MockSystemConfig : public SystemConfig {
 public:
  MOCK_CONST_METHOD0(HasSystemConfig, bool());
  MOCK_CONST_METHOD0(PageSize, int());
  MOCK_CONST_METHOD0(KernelTicksPerSecond, int());
};

}  // namespace common
}  // namespace pl
