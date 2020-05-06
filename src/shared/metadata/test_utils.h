#pragma once

#include <string>
#include <vector>

#include "src/shared/metadata/metadata_filter.h"
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace md {

class TestAgentMetadataFilter : public AgentMetadataFilter {
 public:
  TestAgentMetadataFilter()
      : AgentMetadataFilter(AgentMetadataStateManager::MetadataFilterEntities()) {}

  const std::vector<MetadataType>& inserted_types() const { return inserted_types_; }
  const std::vector<std::string>& inserted_entities() const { return inserted_entities_; }

 protected:
  void InsertEntityImpl(MetadataType key, std::string_view value) override {
    inserted_types_.push_back(key);
    inserted_entities_.push_back(std::string(value));
  }
  bool ContainsEntityImpl(MetadataType key, std::string_view value) const override {
    for (const auto& [i, type] : Enumerate(inserted_types_)) {
      if (type == key && inserted_entities_[i] == value) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<MetadataType> inserted_types_;
  std::vector<std::string> inserted_entities_;
};

}  // namespace md
}  // namespace pl
