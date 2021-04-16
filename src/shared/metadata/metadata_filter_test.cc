#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
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
