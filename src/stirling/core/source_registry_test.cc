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

using ::testing::ElementsAre;
using ::testing::Field;

TEST(SourceRegistryTest, register_sources) {
  SourceRegistry registry;
  registry.RegisterOrDie<SeqGenConnector>("source_0");
  registry.RegisterOrDie<SeqGenConnector>("source_1");

  ASSERT_THAT(registry.sources(),
              ElementsAre(Field(&SourceRegistry::RegistryElement::name, "source_0"),
                          Field(&SourceRegistry::RegistryElement::name, "source_1")));

  // Test that we can use the registry to create a source.
  auto create_source_fn = registry.sources()[0].create_source_fn;
  auto source = create_source_fn("foo");
  EXPECT_EQ(source->name(), "foo");
}

}  // namespace stirling
}  // namespace px
