#pragma once

#include <chrono>

namespace px {
namespace stirling {
namespace testing {

class Clock {
 public:
  virtual uint64_t now() = 0;
  virtual ~Clock() = default;
};

class RealClock : public Clock {
 public:
  uint64_t now() { return std::chrono::steady_clock::now().time_since_epoch().count(); }
};

class MockClock : public Clock {
 public:
  uint64_t now() { return ++t_; }

 private:
  uint64_t t_ = 0;
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
