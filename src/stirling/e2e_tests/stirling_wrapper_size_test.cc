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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"

namespace px {
namespace stirling {

#ifdef __OPTIMIZE__
constexpr uint64_t kFileSizeLimitMB = 118;
#else
constexpr uint64_t kFileSizeLimitMB = 310;
#endif

TEST(StirlingWrapperSizeTest, ExecutableSizeLimit) {
  LOG(INFO) << absl::Substitute("Size limit = $0 MB", kFileSizeLimitMB);
  const std::string stirling_wrapper_path =
      testing::BazelRunfilePath("src/stirling/binaries/stirling_wrapper_core");

  EXPECT_LE(std::filesystem::file_size(std::filesystem::path(stirling_wrapper_path)),
            kFileSizeLimitMB * 1024 * 1024);
}

}  // namespace stirling
}  // namespace px
