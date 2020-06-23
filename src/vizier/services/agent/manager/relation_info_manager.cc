#include "src/vizier/services/agent/manager/relation_info_manager.h"

namespace pl {
namespace vizier {
namespace agent {
Status RelationInfoManager::UpdateRelationInfo(const std::vector<RelationInfo>& relation_info_vec) {
  relation_info_ = relation_info_vec;
  return Status::OK();
}

// TODO(philkuz) (PL-852) only send schema updates for changes to the schema.
void RelationInfoManager::AddSchemaToUpdateInfo(messages::AgentUpdateInfo* update_info) {
  update_info->set_does_update_schema(true);
  for (const auto& relation_info : relation_info_) {
    auto* schema = update_info->add_schema();
    schema->set_name(relation_info.name);
    const table_store::schema::Relation& relation = relation_info.relation;
    if (relation_info.tabletized) {
      schema->set_tabletized(relation_info.tabletized);
      schema->set_tabletization_key(relation.GetColumnName(relation_info.tabletization_key_idx));
    }
    for (size_t i = 0; i < relation.NumColumns(); ++i) {
      auto* column = schema->add_columns();
      column->set_name(relation.GetColumnName(i));
      column->set_data_type(relation.GetColumnType(i));
      column->set_desc(relation.GetColumnDesc(i));
      column->set_semantic_type(relation.GetColumnSemanticType(i));
      // TODO(philkuz) (PL-850) add pattern_type to the relation somehow.
      // column->set_pattern_type(relation.GetColumnPatternType(i));
    }
  }
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
