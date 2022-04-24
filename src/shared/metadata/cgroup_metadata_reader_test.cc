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
#include "src/shared/metadata/cgroup_metadata_reader.h"

#include <absl/container/flat_hash_set.h>

#include "src/common/testing/testing.h"

namespace px {
namespace md {

using ::testing::_;
using ::testing::ReturnArg;

class CGroupMetadataReaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    md_reader_ = std::make_unique<CGroupMetadataReader>("src/shared/metadata/testdata/sysfs1");
  }

  std::unique_ptr<CGroupMetadataReader> md_reader_;
};

TEST_F(CGroupMetadataReaderTest, read_pid_list) {
  absl::flat_hash_set<uint32_t> pid_set;
  ASSERT_OK(md_reader_->ReadPIDs(PodQOSClass::kBestEffort, "abcd", "c123", ContainerType::kDocker,
                                 &pid_set));
  EXPECT_THAT(pid_set, ::testing::UnorderedElementsAre(123, 456, 789));
}

}  // namespace md
}  // namespace px
