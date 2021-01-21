#include <gtest/gtest.h>

#include "src/shared/schema/utils.h"
#include "src/stirling/proto/stirling.pb.h"

namespace pl {

using stirling::stirlingpb::InfoClass;
using stirling::stirlingpb::Subscribe;

TEST(ConvertSubscribeProtoToRelationInfo, test_for_basic_subscription) {
  // Setup a test subscribe message.
  Subscribe subscribe_pb;
  // First info class with two columns.
  auto* info_class = subscribe_pb.add_subscribed_info_classes();
  info_class->set_id(0);
  info_class->set_subscribed(false);

  auto* schema = info_class->mutable_schema();
  schema->set_name("rel1");
  schema->set_desc("a description");

  auto* elem0 = schema->add_elements();
  elem0->set_type(types::INT64);
  elem0->set_name("col1");

  auto* elem1 = info_class->mutable_schema()->add_elements();
  elem1->set_type(types::STRING);
  elem1->set_name("col2");

  // Second relation with one column.
  info_class = subscribe_pb.add_subscribed_info_classes();
  info_class->set_id(1);
  info_class->set_subscribed(false);

  schema = info_class->mutable_schema();
  schema->set_name("rel2");
  schema->set_desc("another description");

  elem0 = schema->add_elements();
  elem0->set_type(types::INT64);
  elem0->set_name("col1_2");

  // Do the conversion.
  const auto relation_info = ConvertSubscribePBToRelationInfo(subscribe_pb);

  // Test the results.
  ASSERT_EQ(2, relation_info.size());

  EXPECT_EQ(2, relation_info[0].relation.NumColumns());
  EXPECT_EQ(1, relation_info[1].relation.NumColumns());

  EXPECT_EQ(types::INT64, relation_info[0].relation.GetColumnType(0));
  EXPECT_EQ("col1", relation_info[0].relation.GetColumnName(0));

  EXPECT_EQ(types::STRING, relation_info[0].relation.GetColumnType(1));
  EXPECT_EQ("col2", relation_info[0].relation.GetColumnName(1));

  EXPECT_EQ(types::INT64, relation_info[1].relation.GetColumnType(0));
  EXPECT_EQ("col1_2", relation_info[1].relation.GetColumnName(0));

  EXPECT_EQ(0, relation_info[0].id);
  EXPECT_EQ(1, relation_info[1].id);

  EXPECT_EQ("rel1", relation_info[0].name);
  EXPECT_EQ("rel2", relation_info[1].name);

  EXPECT_EQ("a description", relation_info[0].desc);
  EXPECT_EQ("another description", relation_info[1].desc);
}

TEST(ConvertSubscribeProtoToRelationInfo, empty_subscribe_should_return_empty) {
  Subscribe subscribe_pb;
  const auto relation_info = ConvertSubscribePBToRelationInfo(subscribe_pb);
  ASSERT_EQ(0, relation_info.size());
}

TEST(ConvertSubscribeProtoToRelationInfo, test_for_tablets_subscription) {
  // Setup a test subscribe message.
  Subscribe subscribe_pb;
  // First info class with two columns.
  auto* info_class = subscribe_pb.add_subscribed_info_classes();
  info_class->set_id(0);
  info_class->set_subscribed(false);

  auto* schema = info_class->mutable_schema();
  schema->set_name("rel1");
  schema->set_desc("a description");

  auto* elem0 = schema->add_elements();
  elem0->set_type(types::INT64);
  elem0->set_name("col1");

  auto* elem1 = schema->add_elements();
  elem1->set_type(types::STRING);
  elem1->set_name("col2");

  schema->set_tabletization_key(1);
  schema->set_tabletized(true);

  // Second relation with one column.
  info_class = subscribe_pb.add_subscribed_info_classes();
  info_class->set_id(1);
  info_class->set_subscribed(false);

  schema = info_class->mutable_schema();
  schema->set_name("rel2");
  schema->set_desc("another description");

  elem0 = schema->add_elements();
  elem0->set_type(types::INT64);
  elem0->set_name("col1_2");

  // Do the conversion.
  const auto relation_info = ConvertSubscribePBToRelationInfo(subscribe_pb);

  // Test the results.
  ASSERT_EQ(2, relation_info.size());

  EXPECT_EQ(2, relation_info[0].relation.NumColumns());
  EXPECT_EQ(1, relation_info[1].relation.NumColumns());

  EXPECT_TRUE(relation_info[0].tabletized);
  EXPECT_FALSE(relation_info[1].tabletized);

  EXPECT_EQ(relation_info[0].tabletization_key_idx, 1);

  EXPECT_EQ("a description", relation_info[0].desc);
  EXPECT_EQ("another description", relation_info[1].desc);
}

}  // namespace pl
