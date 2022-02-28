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
  uint64_t now() override { return std::chrono::steady_clock::now().time_since_epoch().count(); }
};

class MockClock : public Clock {
 public:
  uint64_t now() override { return ++t_; }
  void advance(uint64_t t) { t_ += t; }

 private:
  uint64_t t_ = 0;
};

std::chrono::steady_clock::time_point NanosToTimePoint(uint64_t t) {
  return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(t));
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
