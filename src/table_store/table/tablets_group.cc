#include "src/table_store/table/tablets_group.h"

namespace px {
namespace table_store {

void TabletsGroup::CreateTablet(const types::TabletID& tablet_id) {
  LOG_IF(DFATAL, HasTablet(tablet_id))
      << absl::Substitute("Tablet with id $0 already exists in Table.", tablet_id);
  tablet_id_to_tablet_map_[tablet_id] = Table::Create(relation_);
}

void TabletsGroup::AddTablet(const types::TabletID& tablet_id, std::shared_ptr<Table> tablet) {
  LOG_IF(DFATAL, HasTablet(tablet_id))
      << absl::Substitute("Tablet with id $0 already exists in Table.", tablet_id);
  DCHECK_EQ(tablet->GetRelation(), relation_);
  tablet_id_to_tablet_map_[tablet_id] = tablet;
}

table_store::Table* TabletsGroup::GetTablet(const types::TabletID& tablet_id) const {
  auto tablet_id_to_tablet_map_iter = tablet_id_to_tablet_map_.find(tablet_id);
  if (tablet_id_to_tablet_map_iter == tablet_id_to_tablet_map_.end()) {
    return nullptr;
  }
  return tablet_id_to_tablet_map_iter->second.get();
}

bool TabletsGroup::HasTablet(const types::TabletID& tablet_id) const {
  return tablet_id_to_tablet_map_.find(tablet_id) != tablet_id_to_tablet_map_.end();
}

}  // namespace table_store
}  // namespace px
