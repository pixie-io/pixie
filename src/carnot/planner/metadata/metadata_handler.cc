#include "src/carnot/planner/metadata/metadata_handler.h"

#include <algorithm>

namespace pl {
namespace carnot {
namespace planner {
StatusOr<MetadataProperty*> MetadataHandler::GetProperty(const std::string& md_name) const {
  // looks up in the metadata handler
  // figures out what's available
  auto md_map_it = metadata_map.find(md_name);
  if (md_map_it == metadata_map.end()) {
    return error::InvalidArgument("Metadata '$0' not found", md_name);
  }
  return md_map_it->second;
}
bool MetadataHandler::HasProperty(const std::string& md_name) const {
  auto md_map_it = metadata_map.find(md_name);
  return md_map_it != metadata_map.end();
}
std::unique_ptr<MetadataHandler> MetadataHandler::Create() {
  std::unique_ptr<MetadataHandler> handler(new MetadataHandler());
  handler->AddObject<IdMetadataProperty>(MetadataType::CONTAINER_ID, {}, {MetadataType::UPID});
  handler->AddObject<IdMetadataProperty>(MetadataType::SERVICE_ID, {},
                                         {MetadataType::UPID, MetadataType::SERVICE_NAME});
  handler->AddObject<IdMetadataProperty>(MetadataType::POD_ID, {},
                                         {MetadataType::UPID, MetadataType::POD_NAME});
  handler->AddObject<IdMetadataProperty>(MetadataType::DEPLOYMENT_ID, {},
                                         {MetadataType::UPID, MetadataType::DEPLOYMENT_NAME});
  handler->AddObject<NameMetadataProperty>(MetadataType::SERVICE_NAME, {"service"},
                                           {MetadataType::UPID, MetadataType::SERVICE_ID});
  handler->AddObject<NameMetadataProperty>(MetadataType::POD_NAME, {"pod"},
                                           {MetadataType::UPID, MetadataType::POD_ID});
  handler->AddObject<NameMetadataProperty>(MetadataType::DEPLOYMENT_NAME, {"deployment"},
                                           {MetadataType::UPID, MetadataType::DEPLOYMENT_ID});
  handler->AddObject<NameMetadataProperty>(MetadataType::NAMESPACE, {}, {MetadataType::UPID});
  handler->AddObject<NameMetadataProperty>(MetadataType::NODE_NAME, {"node"}, {MetadataType::UPID});
  handler->AddObject<NameMetadataProperty>(MetadataType::HOSTNAME, {"host"}, {MetadataType::UPID});
  handler->AddObject<NameMetadataProperty>(MetadataType::CONTAINER_NAME, {"container"},
                                           {MetadataType::UPID});
  handler->AddObject<NameMetadataProperty>(MetadataType::CMDLINE, {"cmd"}, {MetadataType::UPID});
  handler->AddObject<NameMetadataProperty>(MetadataType::ASID, {}, {MetadataType::UPID});
  handler->AddObject<Int64MetadataProperty>(MetadataType::PID, {}, {MetadataType::UPID});
  return handler;
}

MetadataProperty* MetadataHandler::AddProperty(std::unique_ptr<MetadataProperty> md_property) {
  MetadataProperty* raw_property = md_property.get();
  property_pool.push_back(std::move(md_property));
  return raw_property;
}
void MetadataHandler::AddMapping(const std::string& name, MetadataProperty* property) {
  metadata_map.emplace(name, property);
}
template <typename Property>
void MetadataHandler::AddObject(MetadataType md_type, const std::vector<std::string>& aliases,
                                const std::vector<MetadataType>& key_metadata) {
  MetadataProperty* raw_property = AddProperty(std::make_unique<Property>(md_type, key_metadata));
  std::string md_name = MetadataProperty::GetMetadataString(md_type);
  absl::AsciiStrToLower(&md_name);
  DCHECK(!HasProperty(md_name)) << absl::Substitute("Metadata already exists for key '$0'.",
                                                    md_name);
  AddMapping(md_name, raw_property);
  for (const auto& a : aliases) {
    DCHECK(!HasProperty(a)) << absl::Substitute("Metadata already exists for key '$0'.", a);
    AddMapping(a, raw_property);
  }
}

}  // namespace planner
}  // namespace carnot
}  // namespace pl
