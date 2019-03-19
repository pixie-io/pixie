#include "src/agent/controller/executor.h"

#include <arrow/pretty_print.h>

#include <functional>
#include <sstream>
#include <unordered_map>

#include "src/common/scoped_timer.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/vizier/proto/service.pb.h"

namespace pl {
namespace agent {
using carnot::exec::Column;
using carnot::exec::Table;
using pl::types::DataType;

namespace {
// TODO(zasgar): Move this utility outside of here. This involves changing the proto definititons to
// move row batches outside of vizier protos.
/**
 * Converts an arrow batch to a proto row batch.
 * @param rb The rowbatch.
 * @param relation The relation.
 * @param rb_data_pb The output proto.
 * @return Status of conversion.
 */
Status ConvertRecordBatchToProto(arrow::RecordBatch* rb, const pl::carnot::plan::Relation& relation,
                                 pl::vizier::RowBatchData* rb_data_pb) {
  for (int col_idx = 0; col_idx < rb->num_columns(); ++col_idx) {
    auto col = rb->column(col_idx);
    switch (relation.GetColumnType(col_idx)) {
      case DataType::TIME64NS: {
        auto output_col_data = rb_data_pb->add_cols();
        auto output_data_casted = output_col_data->mutable_time64ns_data();
        for (int64_t i = 0; i < col->length(); ++i) {
          output_data_casted->add_data(
              pl::carnot::udf::GetValue(reinterpret_cast<arrow::Int64Array*>(col.get()), i));
        }
        break;
      }
      case DataType::STRING: {
        auto output_col_data = rb_data_pb->add_cols();
        auto output_data_casted = output_col_data->mutable_string_data();
        for (int64_t i = 0; i < col->length(); ++i) {
          output_data_casted->add_data(
              pl::carnot::udf::GetValue(reinterpret_cast<arrow::StringArray*>(col.get()), i));
        }
        break;
      }
      case DataType::INT64: {
        auto output_col_data = rb_data_pb->add_cols();
        auto output_data_casted = output_col_data->mutable_int64_data();
        for (int64_t i = 0; i < col->length(); ++i) {
          output_data_casted->add_data(
              pl::carnot::udf::GetValue(reinterpret_cast<arrow::Int64Array*>(col.get()), i));
        }
        break;
      }
      case DataType::FLOAT64: {
        auto output_col_data = rb_data_pb->add_cols();
        auto output_data_casted = output_col_data->mutable_float64_data();
        for (int64_t i = 0; i < col->length(); ++i) {
          output_data_casted->add_data(
              pl::carnot::udf::GetValue(reinterpret_cast<arrow::DoubleArray*>(col.get()), i));
        }
        break;
      }
      case DataType::BOOLEAN: {
        auto output_col_data = rb_data_pb->add_cols();
        auto output_data_casted = output_col_data->mutable_boolean_data();
        for (int64_t i = 0; i < col->length(); ++i) {
          output_data_casted->add_data(
              pl::carnot::udf::GetValue(reinterpret_cast<arrow::BooleanArray*>(col.get()), i));
        }
        break;
      }
      default:
        CHECK(0) << "Unsupported Data Type";
    }
  }
  return Status::OK();
}

}  // namespace

// Temporary and to be replaced by data table from Stirling and Executor
Status Executor::AddDummyTable(const std::string& name,
                               std::shared_ptr<carnot::exec::Table> table) {
  carnot_->table_store()->AddTable(name, table);
  return Status::OK();
}

Status Executor::ServiceQuery(const std::string& query,
                              pl::vizier::AgentQueryResponse* query_resp_pb,
                              types::Time64NSValue time_now) {
  carnot::CarnotQueryResult ret;
  {
    ScopedTimer query_timer("query timer");
    PL_ASSIGN_OR_RETURN(ret, carnot_->ExecuteQuery(query, time_now));
  }
  // Currently ignore many output tables until it's necessary.
  CHECK_EQ(ret.NumTables(), 1ULL);

  PL_ASSIGN_OR_RETURN(auto record_batches, ret.GetTableAsRecordBatches(0));
  auto relation = ret.GetTable(0)->GetRelation();
  auto output_table = query_resp_pb->add_tables();
  auto output_relation = output_table->mutable_relation();
  for (const auto rb : record_batches) {
    PL_RETURN_IF_ERROR(ConvertRecordBatchToProto(rb.get(), relation, output_table->add_data()));
  }
  for (size_t i = 0; i < relation.NumColumns(); ++i) {
    auto* col = output_relation->add_columns();
    *col->mutable_column_name() = relation.GetColumnName(i);
    col->set_column_type(relation.GetColumnType(i));
  }
  auto* stats_pb = query_resp_pb->mutable_stats();
  stats_pb->set_bytes_processed(ret.bytes_processed);
  auto* timing_pb = stats_pb->mutable_timing();
  timing_pb->set_compilation_time_ns(ret.compile_time_ns);
  timing_pb->set_execution_time_ns(ret.exec_time_ns);

  return Status::OK();
}

Status Executor::Init() {
  PL_RETURN_IF_ERROR(carnot_->Init());
  PL_RETURN_IF_ERROR(stirling_->Init());
  auto table_store = carnot_->table_store();
  stirling_->RegisterCallback(std::bind(&carnot::exec::TableStore::AppendData, table_store,
                                        std::placeholders::_1, std::placeholders::_2));
  return Status::OK();
}

void Executor::GeneratePublishMessage(Publish* publish_pb) {
  stirling_->GetPublishProto(publish_pb);
}

Subscribe Executor::SubscribeToEverything(const Publish& publish_proto) {
  return stirling::SubscribeToAllInfoClasses(publish_proto);
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
