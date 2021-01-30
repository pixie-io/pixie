#pragma once

#include <string>
#include <vector>

#include "src/shared/metadata/metadata_filter.h"
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace md {

class TestAgentMetadataFilter : public AgentMetadataFilter {
 public:
  TestAgentMetadataFilter() : AgentMetadataFilter(kMetadataFilterEntities) {}
  const std::vector<std::string>& inserted_entities() const { return inserted_entities_; }

 protected:
  void Insert(std::string_view value) override { inserted_entities_.push_back(std::string(value)); }
  bool Contains(std::string_view value) const override {
    return std::find(inserted_entities_.begin(), inserted_entities_.end(), value) !=
           inserted_entities_.end();
  }
  MetadataInfo ToProtoImpl() const override {
    CHECK(false) << "Unimplemented method ToProtoImpl for test class TestAgentMetadataFilter.";
  }

 private:
  std::vector<std::string> inserted_entities_;
};

}  // namespace md
}  // namespace pl
