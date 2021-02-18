#include <string>
#include <vector>

#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/shared/metadata/metadata_filter.h"

namespace pl {
namespace md {

StatusOr<std::unique_ptr<AgentMetadataFilter>> AgentMetadataFilter::Create(
    int64_t max_entries, double error_rate, const absl::flat_hash_set<MetadataType>& entity_types) {
  return AgentMetadataFilterImpl::Create(max_entries, error_rate, entity_types);
}

StatusOr<std::unique_ptr<AgentMetadataFilter>> AgentMetadataFilter::FromProto(
    const MetadataInfo& proto) {
  switch (proto.filter_case()) {
    case MetadataInfo::FilterCase::FILTER_NOT_SET:
      return error::Internal("Received an improperly formatted MetadataInfo with no filter set.");
    case MetadataInfo::FilterCase::kXxhash64BloomFilter:
      return AgentMetadataFilterImpl::FromProto(proto);
    default:
      return error::Internal("Unknown filter case.");
  }
}

std::string ToEntityKeyPair(MetadataType type, std::string_view entity) {
  return absl::Substitute("$0=$1", MetadataType_Name(type), entity);
}

Status AgentMetadataFilter::InsertEntity(MetadataType key, std::string_view value) {
  epoch_id_++;
  if (!metadata_types_.contains(key)) {
    return error::Internal("Metadata type $0 is not registered in AgentMetadataFilter.", key);
  }
  Insert(ToEntityKeyPair(key, value));
  return Status::OK();
}

bool AgentMetadataFilter::ContainsEntity(MetadataType key, std::string_view value) const {
  if (!metadata_types_.contains(key)) {
    return false;
  }
  return Contains(ToEntityKeyPair(key, value));
}

MetadataInfo AgentMetadataFilter::ToProto() {
  auto output = ToProtoImpl();
  for (const auto& type : metadata_types_) {
    output.add_metadata_fields(type);
  }
  return output;
}

void AgentMetadataFilterImpl::Insert(std::string_view val) { bloomfilter_->Insert(val); }

bool AgentMetadataFilterImpl::Contains(std::string_view val) const {
  return bloomfilter_->Contains(val);
}

MetadataInfo AgentMetadataFilterImpl::ToProtoImpl() const {
  MetadataInfo output;
  *(output.mutable_xxhash64_bloom_filter()) = bloomfilter_->ToProto();
  return output;
}

StatusOr<std::unique_ptr<AgentMetadataFilter>> AgentMetadataFilterImpl::FromProto(
    const MetadataInfo& proto) {
  DCHECK_EQ(proto.filter_case(), MetadataInfo::FilterCase::kXxhash64BloomFilter);
  PL_ASSIGN_OR_RETURN(auto bf, XXHash64BloomFilter::FromProto(proto.xxhash64_bloom_filter()));
  absl::flat_hash_set<MetadataType> types;
  for (auto i = 0; i < proto.metadata_fields_size(); ++i) {
    types.insert(proto.metadata_fields(i));
  }
  return std::unique_ptr<AgentMetadataFilter>(new AgentMetadataFilterImpl(std::move(bf), types));
}

}  // namespace md
}  // namespace pl
