#include "src/carnot/compiler/metadata_handler.h"
namespace pl {
namespace carnot {
namespace compiler {
StatusOr<MetadataProperty*> MetadataHandler::GetProperty(const std::string& md_name) const {
  // looks up in the metadata handler
  // figures out what's available
  auto md_map_it = metadata_map.find(md_name);
  if (md_map_it == metadata_map.end()) {
    return error::InvalidArgument("Metadata '$0' not found");
  }
  return md_map_it->second;
}
bool MetadataHandler::HasProperty(const std::string& md_name) const {
  auto md_map_it = metadata_map.find(md_name);
  return md_map_it != metadata_map.end();
}
std::unique_ptr<MetadataHandler> MetadataHandler::Create() {
  std::unique_ptr<MetadataHandler> handler(new MetadataHandler());
  handler->AddObject<IdMetadataProperty>("service_id", {}, {});
  handler->AddObject<IdMetadataProperty>("pod_id", {}, {});
  handler->AddObject<IdMetadataProperty>("container", {}, {});
  handler->AddObject<NameMetadataProperty>("service_name", {"service"}, {"service_id"});
  handler->AddObject<NameMetadataProperty>("pod_name", {"pod"}, {"pod_id"});
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
void MetadataHandler::AddObject(const std::string& md_name, const std::vector<std::string>& aliases,
                                const std::vector<std::string>& key_columns) {
  MetadataProperty* raw_property = AddProperty(std::make_unique<Property>(md_name, key_columns));
  DCHECK(!HasProperty(md_name)) << absl::Substitute("Metadata already exists for key '$0'.",
                                                    md_name);
  AddMapping(md_name, raw_property);
  for (const auto& a : aliases) {
    DCHECK(!HasProperty(a)) << absl::Substitute("Metadata already exists for key '$0'.", a);
    AddMapping(a, raw_property);
  }
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
