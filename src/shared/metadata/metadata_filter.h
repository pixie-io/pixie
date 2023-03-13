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

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/bloomfilter/bloomfilter.h"
#include "src/shared/metadatapb/metadata.pb.h"

namespace px {
namespace md {

using bloomfilter::XXHash64BloomFilter;
using carnot::planner::distributedpb::MetadataInfo;
using shared::metadatapb::MetadataType;
using shared::metadatapb::MetadataType_Name;

const absl::flat_hash_set<MetadataType> kMetadataFilterEntities = {
    MetadataType::SERVICE_ID, MetadataType::SERVICE_NAME, MetadataType::POD_ID,
    MetadataType::POD_NAME, MetadataType::CONTAINER_ID};

/**
 * An abstract class that keeps track of the various entities in this metadata state.
 * It will be extended by another class that backs it with a data structure such as
 * a bloom filter to store all of the entities in this state.
 */
class AgentMetadataFilter {
 public:
  /**
   * Factor function to create an AgentMetadataFilter.
   * @param max_entries The maximum number of entries that can be stored while preserving the false
   * positive rate.
   * @param error_rate The false positive rate for this filter.
   * @param entity_types The types of entities that are stored in this filter.
   */
  static StatusOr<std::unique_ptr<AgentMetadataFilter>> Create(
      int64_t max_entries, double error_rate,
      const absl::flat_hash_set<MetadataType>& entity_types);
  static StatusOr<std::unique_ptr<AgentMetadataFilter>> FromProto(const MetadataInfo& proto);
  MetadataInfo ToProto();
  virtual ~AgentMetadataFilter() = default;

  /**
   * Construct a metadata filter that stores all metadata entities belonging to 'types'.
   */
  explicit AgentMetadataFilter(const absl::flat_hash_set<MetadataType>& types)
      : metadata_types_(types) {}

  /**
   * Insert a metadata entity by key and value into the filter.
   */
  Status InsertEntity(MetadataType key, std::string_view value);

  /**
   * Check whether the filter contains a particular key/value pair.
   */
  bool ContainsEntity(MetadataType key, std::string_view value) const;

  /**
   * Get the registered metadata keys that are stored in this filter.
   */
  absl::flat_hash_set<MetadataType> metadata_types() const { return metadata_types_; }
  // Used to track changes in the filter.
  int64_t epoch_id() const { return epoch_id_; }

 protected:
  virtual void Insert(std::string_view value) = 0;
  virtual bool Contains(std::string_view value) const = 0;

  /**
   * Creates an proto, excluding the metadata_fields field which is taken care of by the
   * public ToProto() method.
   */
  virtual MetadataInfo ToProtoImpl() const = 0;

 private:
  absl::flat_hash_set<MetadataType> metadata_types_;
  int64_t epoch_id_ = 0;
};

/**
 * An implementation of AgentMetadataFilter backed by an XXHASH64-based bloom filter.
 */
class AgentMetadataFilterImpl : public AgentMetadataFilter {
 public:
  static StatusOr<std::unique_ptr<AgentMetadataFilter>> Create(
      int64_t max_entries, double error_rate, const absl::flat_hash_set<MetadataType>& types) {
    PX_ASSIGN_OR_RETURN(auto bf, XXHash64BloomFilter::Create(max_entries, error_rate));
    return std::unique_ptr<AgentMetadataFilter>(new AgentMetadataFilterImpl(std::move(bf), types));
  }

  static StatusOr<std::unique_ptr<AgentMetadataFilter>> FromProto(const MetadataInfo& proto);

 protected:
  void Insert(std::string_view entity) override;
  bool Contains(std::string_view entity) const override;
  MetadataInfo ToProtoImpl() const override;

 private:
  AgentMetadataFilterImpl(std::unique_ptr<XXHash64BloomFilter> bloomfilter,
                          const absl::flat_hash_set<MetadataType>& types)
      : AgentMetadataFilter(types), bloomfilter_(std::move(bloomfilter)) {}

  std::unique_ptr<XXHash64BloomFilter> bloomfilter_;
};

}  // namespace md
}  // namespace px
