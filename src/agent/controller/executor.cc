#include "src/agent/controller/executor.h"

#include <arrow/pretty_print.h>

#include <functional>
#include <sstream>

namespace pl {
namespace agent {
using carnot::exec::Column;
using carnot::exec::Table;

// Temporary and to be replaced by data table from Stirling and Executor
Status Executor::AddDummyTable(const std::string& name,
                               std::shared_ptr<carnot::exec::Table> table) {
  carnot_->table_store()->AddTable(name, table);
  return Status::OK();
}

std::string Executor::RecordBatchToStr(RecordBatchSPtr ptr) {
  std::stringstream ss;
  PL_CHECK_OK(arrow::PrettyPrint(*ptr, 0, &ss));
  return ss.str();
}

StatusOr<std::vector<std::string>> Executor::ServiceQueryAsString(const std::string& query) {
  PL_ASSIGN_OR_RETURN(carnot::CarnotQueryResult ret, carnot_->ExecuteQuery(query));
  // Currently ignore many output tables until it's necessary.
  CHECK_EQ(ret.NumTables(), 1ULL);

  PL_ASSIGN_OR_RETURN(auto record_batches, ret.GetTableAsRecordBatches(0));

  std::vector<std::string> batches_as_string;
  for (const auto rb : record_batches) {
    batches_as_string.push_back(RecordBatchToStr(rb));
  }
  return batches_as_string;
}

Status Executor::Init() {
  PL_RETURN_IF_ERROR(carnot_->Init());
  PL_RETURN_IF_ERROR(stirling_->Init());
  stirling_->RegisterCallback(std::bind(&carnot::exec::TableStore::AppendData,
                                        *(carnot_->table_store()), std::placeholders::_1,
                                        std::placeholders::_2));
  return Status::OK();
}

void Executor::GeneratePublishMessage(Publish* publish_pb) {
  stirling_->GetPublishProto(publish_pb);
}

Subscribe Executor::SubscribeToEverything(const Publish& publish_proto) {
  return stirling::SubscribeToAllElements(publish_proto);
}

Status Executor::CreateTablesFromSubscription(const Subscribe& subscribe_proto) {
  // Set up the data tables in Stirling based on subscription
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_proto));
  // Create tables in Carnot based on subscription message. Only create tables
  // from info schemas that have elements.
  auto table_store = carnot_->table_store();
  for (const auto& info_class : subscribe_proto.subscribed_info_classes()) {
    if (info_class.elements_size() > 0) {
      auto relation = InfoClassProtoToRelation(info_class);
      PL_RETURN_IF_ERROR(table_store->AddTable(info_class.name(), info_class.id(),
                                               std::make_shared<carnot::exec::Table>(relation)));
    }
  }
  return Status::OK();
}

Relation Executor::InfoClassProtoToRelation(const InfoClass& info_class_proto) {
  std::vector<types::DataType> col_types;
  std::vector<std::string> col_names;
  for (const auto& element : info_class_proto.elements()) {
    col_types.push_back(element.type());
    col_names.push_back(element.name());
  }
  return carnot::plan::Relation(col_types, col_names);
}

}  // namespace agent
}  // namespace pl
