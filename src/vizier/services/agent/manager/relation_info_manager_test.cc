#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

#include "src/vizier/services/agent/manager/relation_info_manager.h"

#include "src/common/testing/testing.h"

namespace pl {
namespace vizier {
namespace agent {

using ::pl::table_store::schema::Relation;
using ::pl::testing::proto::EqualsProto;

class RelationInfoManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { relation_info_manager_ = std::make_unique<RelationInfoManager>(); };

  std::unique_ptr<RelationInfoManager> relation_info_manager_;
};

const char* kAgentUpdateInfoSchemaNoTablets = R"proto(
does_update_schema: true
schema {
  name: "relation0"
  desc: "desc0"  
  columns {
    name: "time_"
    data_type: TIME64NS
    semantic_type: ST_NONE
  }
  columns {
    name: "count"
    data_type: INT64
    semantic_type: ST_NONE
  }
}
schema {
  name: "relation1"
  desc: "desc1"
  columns {
    name: "time_"
    data_type: TIME64NS
    semantic_type: ST_NONE
  }
  columns {
    name: "gauge"
    data_type: FLOAT64
    semantic_type: ST_NONE
  }
})proto";

TEST_F(RelationInfoManagerTest, test_update) {
  // Relation info with no tabletization.
  Relation relation0({types::TIME64NS, types::INT64}, {"time_", "count"});
  RelationInfo relation_info0("relation0", /* id */ 0, "desc0", relation0);

  // Relation info with no tabletization.
  Relation relation1({types::TIME64NS, types::FLOAT64}, {"time_", "gauge"});
  RelationInfo relation_info1("relation1", /* id */ 1, "desc1", relation1);

  EXPECT_OK(relation_info_manager_->AddRelationInfo(std::move(relation_info0)));
  EXPECT_OK(relation_info_manager_->AddRelationInfo(std::move(relation_info1)));

  // Check to see that the agent info is as expected.
  messages::AgentUpdateInfo update_info;
  relation_info_manager_->AddSchemaToUpdateInfo(&update_info);
  EXPECT_THAT(update_info, EqualsProto(kAgentUpdateInfoSchemaNoTablets));
}

const char* kAgentUpdateInfoSchemaHasTablets = R"proto(
does_update_schema: true
schema {
  name: "relation0"
  desc: "desc0"  
  columns {
    name: "time_"
    data_type: TIME64NS
    semantic_type: ST_NONE
  }
  columns {
    name: "count"
    data_type: INT64
    semantic_type: ST_NONE
  }
}
schema {
  name: "relation1"
  desc: "desc1"  
  columns {
    name: "time_"
    data_type: TIME64NS
    semantic_type: ST_NONE
  }
  columns {
    name: "upid"
    data_type: UINT128
    semantic_type: ST_NONE
  }
  columns {
    name: "count"
    data_type: INT64
    semantic_type: ST_NONE
  }
  tabletized: true
  tabletization_key: "upid"
})proto";

TEST_F(RelationInfoManagerTest, test_tabletization_keys) {
  // Relation info with no tabletization.
  Relation relation0({types::TIME64NS, types::INT64}, {"time_", "count"});
  RelationInfo relation_info0("relation0", /* id */ 0, "desc0", relation0);

  // Relation info with a tablet key ("upid").
  Relation relation1({types::TIME64NS, types::UINT128, types::INT64}, {"time_", "upid", "count"});
  RelationInfo relation_info1("relation1", /* id */ 1, "desc1", /* tabletization_key_idx */ 1,
                              relation1);

  EXPECT_FALSE(relation_info_manager_->has_updates());

  // Pass relation info to the manager.
  EXPECT_OK(relation_info_manager_->AddRelationInfo(std::move(relation_info0)));
  EXPECT_TRUE(relation_info_manager_->has_updates());

  messages::AgentUpdateInfo update_info0;
  relation_info_manager_->AddSchemaToUpdateInfo(&update_info0);
  EXPECT_FALSE(relation_info_manager_->has_updates());

  EXPECT_OK(relation_info_manager_->AddRelationInfo(std::move(relation_info1)));
  EXPECT_TRUE(relation_info_manager_->has_updates());

  // Check to see that the agent info is as expected.
  messages::AgentUpdateInfo update_info;
  relation_info_manager_->AddSchemaToUpdateInfo(&update_info);
  EXPECT_THAT(update_info, EqualsProto(kAgentUpdateInfoSchemaHasTablets));
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
