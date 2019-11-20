#pragma once
#include <vector>

#include "src/shared/schema/utils.h"
#include "src/vizier/messages/messagespb/messages.pb.h"

namespace pl {
namespace vizier {
namespace agent {
/**
 * @brief Manager of relation info for a given agent. In the future, this can be used to provide
 * diff-updates of a schema rather than passing the entire schema through the update message.
 */
class RelationInfoManager {
 public:
  /**
   * @brief Receives an updated record of relation info for the agent.
   *
   * @param relation_info_vec: the new relation info vector for the agent.
   * @return Status: Error if relation info vector is malformed.
   */
  Status UpdateRelationInfo(const std::vector<RelationInfo>& relation_info_vec);

  /**
   * @brief Adds schema updates to the update_info message.
   *
   * @param update_info: the message that should receive the updated schema info.
   */
  void AddSchemaToUpdateInfo(messages::AgentUpdateInfo* update_info);

 private:
  std::vector<RelationInfo> relation_info_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
