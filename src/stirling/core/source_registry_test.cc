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

#include "src/stirling/core/source_registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"

namespace px {
namespace stirling {

void RegisterTestSources(SourceRegistry* registry) {
  registry->RegisterOrDie<SeqGenConnector>("source_0");
  registry->RegisterOrDie<SeqGenConnector>("source_1");
}

class SourceRegistryTest : public ::testing::Test {
 protected:
  SourceRegistryTest() = default;
  void SetUp() override { RegisterTestSources(&registry_); }
  SourceRegistry registry_;
};

TEST_F(SourceRegistryTest, register_sources) {
  std::string name;
  {
    name = "fake_proc_stat";
    auto iter = registry_.sources().find("source_0");
    auto element = iter->second;
    ASSERT_NE(registry_.sources().end(), iter);
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->name());
  }

  {
    name = "proc_stat";
    auto iter = registry_.sources().find("source_1");
    ASSERT_NE(registry_.sources().end(), iter);
    auto element = iter->second;
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->name());
  }

  auto all_sources = registry_.sources();
  EXPECT_EQ(2, all_sources.size());
}

}  // namespace stirling
}  // namespace px
