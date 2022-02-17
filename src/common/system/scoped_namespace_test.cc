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

#include <filesystem>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/system/scoped_namespace.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

using ::testing::Contains;
using ::testing::Not;

namespace px {
namespace system {

class TestContainer : public ContainerRunner {
 public:
  TestContainer()
      : ContainerRunner(px::testing::BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/common/system/testdata/test_container_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "test_container";
  static constexpr std::string_view kReadyMessage = "started";
};

std::vector<std::filesystem::path> ListDir(const std::filesystem::path& dir) {
  using DirectoryIterator = std::filesystem::directory_iterator;
  return std::vector<std::filesystem::path>(DirectoryIterator(dir), DirectoryIterator());
}

// Use mount namespace as a demonstration of ScopedNamespace functionality.
TEST(ScopedNamespaceTest, MountNamespace) {
  TestContainer container;
  ASSERT_OK(container.Run());

  // This is a path that we can reasonably assume only exists in the TestContainer.
  const std::filesystem::path kTestContainerDir("/files_dir");

  EXPECT_THAT(ListDir("/"), Not(Contains(kTestContainerDir)));

  // Create a scope under which we will switch namespaces.
  // The namespace will apply to all code in the scope, but once it exits,
  // the original namespace is restored (RAII style).
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ScopedNamespace> scoped_namespace,
                         ScopedNamespace::Create(container.process_pid(), "mnt"));

    // Now that we're in the container's mount namespace, we expect to see its filesystem.
    EXPECT_THAT(ListDir("/"), Contains(kTestContainerDir));
  }

  EXPECT_THAT(ListDir("/"), Not(Contains(kTestContainerDir)));
}

}  // namespace system
}  // namespace px
