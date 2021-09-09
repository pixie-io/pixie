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

#include <string>

#include <gmock/gmock.h>

#include "src/common/system/config.h"

namespace px {
namespace system {

class MockConfig : public Config {
 public:
  MOCK_CONST_METHOD0(HasConfig, bool());
  MOCK_CONST_METHOD0(PageSize, int64_t());
  MOCK_CONST_METHOD0(KernelTicksPerSecond, int64_t());
  MOCK_CONST_METHOD1(ConvertToRealTime, uint64_t(uint64_t val));
  MOCK_CONST_METHOD0(sysfs_path, const std::filesystem::path&());
  MOCK_CONST_METHOD0(host_path, const std::filesystem::path&());
  MOCK_CONST_METHOD0(proc_path, const std::filesystem::path&());
  MOCK_CONST_METHOD1(ToHostPath, std::filesystem::path(const std::filesystem::path& p));
};

}  // namespace system
}  // namespace px
