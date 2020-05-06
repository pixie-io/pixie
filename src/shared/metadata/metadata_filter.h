#pragma once

#include <string>
#include <vector>

#include "src/shared/metadatapb/metadata.pb.h"

namespace pl {
namespace md {

using shared::metadatapb::MetadataType;

/**
 * An abstract class that keeps track of the various entities in this metadata state.
 * It will be extended by another class that backs it with a data structure such as
 * a bloom filter to store all of the entities in this state.
 */
class AgentMetadataFilter {
 public:
  /**
   * Construct a metadata filter that stores all metadata entities belonging to 'types'.
   */
  explicit AgentMetadataFilter(const absl::flat_hash_set<MetadataType>& types)
      : metadata_types_(types) {}

  /**
   * Insert a metadata entity by key and value into the filter.
   */
  Status InsertEntity(MetadataType key, std::string_view value) {
    if (!metadata_types_.contains(key)) {
      return error::Internal("Metadata type $0 is not registered in AgentMetadataFilter.", key);
    }
    InsertEntityImpl(key, value);
    return Status::OK();
  }

  /**
   * Check whether the filter contains a particular key/value pair.
   */
  bool ContainsEntity(MetadataType key, std::string_view value) const {
    if (!metadata_types_.contains(key)) {
      return false;
    }
    return ContainsEntityImpl(key, value);
  }

  /**
   * Get the registered metadata keys that are stored in this filter.
   */
  absl::flat_hash_set<MetadataType> metadata_types() const { return metadata_types_; }

  /**
   * Utility to turn a kubernetes object name into a name prepended with namespace.
   * This can be removed when the query language no longer expresses kubernetes
   * names with the namespace prepended.
   */
  static std::string WithK8sNamespace(std::string_view ns, std::string_view entity) {
    return absl::Substitute("$0/$1", ns, entity);
  }

 protected:
  virtual void InsertEntityImpl(MetadataType key, std::string_view value) = 0;
  virtual bool ContainsEntityImpl(MetadataType key, std::string_view value) const = 0;

 private:
  absl::flat_hash_set<MetadataType> metadata_types_;
};

}  // namespace md
}  // namespace pl
