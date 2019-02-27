#include <functional>

#include "src/agent/controller/executor.h"

namespace pl {
namespace agent {

Status Executor::Init() {
  PL_RETURN_IF_ERROR(carnot_->Init());
  PL_RETURN_IF_ERROR(stirling_->Init());
  stirling_->RegisterCallback(std::bind(&carnot::exec::TableStore::AppendData,
                                        *(carnot_->table_store()), std::placeholders::_1,
                                        std::placeholders::_2));
  return Status::OK();
}

Publish Executor::GeneratePublishMessage() { return stirling_->GetPublishProto(); }

Subscribe Executor::SubscribeToEverything(const Publish& publish_proto) {
  return stirling::SubscribeToAllElements(publish_proto);
}

Status Executor::CreateTablesFromSubscription(
    const stirling::stirlingpb::Subscribe& subscribe_proto) {
  // Set up the data tables in Stirling based on subscription
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_proto));
  // Create tables in Carnot based on subscription message. Only create tables
  // from info schemas that have elements.
  for (const auto& info_class : subscribe_proto.subscribed_info_classes()) {
    std::vector<types::DataType> col_types;
    std::vector<std::string> col_names;
    if (info_class.elements_size() > 0) {
      for (const auto& element : info_class.elements()) {
        col_types.push_back(element.type());
        col_names.push_back(element.name());
      }
      carnot::plan::Relation relation(col_types, col_names);
      auto table_store = carnot_->table_store();
      PL_RETURN_IF_ERROR(table_store->AddTable(info_class.name(), info_class.id(),
                                               std::make_shared<carnot::exec::Table>(relation)));
    }
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace pl
