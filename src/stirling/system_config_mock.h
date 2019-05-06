#pragma once

#include <gmock/gmock.h>

namespace pl {
namespace stirling {

class MockSystemConfig : public SystemConfig {
 public:
  MOCK_METHOD0(HasSystemConfig, bool());
  MOCK_METHOD0(PageSize, int());
  MOCK_METHOD0(KernelTicksPerSecond, int());
};

}  // namespace stirling
}  // namespace pl
