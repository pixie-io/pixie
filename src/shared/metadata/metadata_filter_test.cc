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

#include "src/common/testing/testing.h"
#include "src/shared/metadata/metadata_filter.h"

namespace px {
namespace md {

using ::testing::UnorderedElementsAre;

TEST(AgentMetadataFilter, test_basic) {
  auto filter =
      AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_NAME, MetadataType::CONTAINER_ID})
          .ConsumeValueOrDie();
  EXPECT_THAT(filter->metadata_types(),
              UnorderedElementsAre(MetadataType::POD_NAME, MetadataType::CONTAINER_ID));

  // basic insertion
  EXPECT_FALSE(filter->ContainsEntity(MetadataType::POD_NAME, "foo"));
  EXPECT_OK(filter->InsertEntity(MetadataType::POD_NAME, "foo"));
  EXPECT_TRUE(filter->ContainsEntity(MetadataType::POD_NAME, "foo"));
  EXPECT_FALSE(filter->ContainsEntity(MetadataType::CONTAINER_ID, "foo"));
  EXPECT_FALSE(filter->ContainsEntity(MetadataType::POD_NAME, "bar"));
  EXPECT_NOT_OK(filter->InsertEntity(MetadataType::SERVICE_NAME, "abc"));
}

TEST(AgentMetadataFilter, test_proto) {
  auto filter =
      AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_NAME, MetadataType::CONTAINER_ID})
          .ConsumeValueOrDie();
  EXPECT_OK(filter->InsertEntity(MetadataType::POD_NAME, "foo"));

  auto serialized = filter->ToProto();
  auto deserialized = AgentMetadataFilter::FromProto(serialized).ConsumeValueOrDie();
  EXPECT_THAT(deserialized->metadata_types(),
              UnorderedElementsAre(MetadataType::POD_NAME, MetadataType::CONTAINER_ID));
  EXPECT_TRUE(deserialized->ContainsEntity(MetadataType::POD_NAME, "foo"));
  EXPECT_FALSE(deserialized->ContainsEntity(MetadataType::CONTAINER_ID, "foo"));
  EXPECT_FALSE(deserialized->ContainsEntity(MetadataType::POD_NAME, "bar"));
}

}  // namespace md
}  // namespace px
