#include <utility>
#include <vector>

#include "src/carnot/exec/table_store.h"
#include "src/stirling/bpftrace_connector.h"

namespace pl {
namespace carnot {
namespace exec {

std::shared_ptr<std::unordered_map<std::string, plan::Relation>> TableStore::GetRelationMap() {
  auto map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  map->reserve(table_name_to_table_map_.size());
  for (const auto& table : table_name_to_table_map_) {
    map->emplace(table.first, table.second->GetRelation());
  }
  return map;
}

Status TableStore::AppendData(
    uint64_t table_id, std::unique_ptr<pl::stirling::ColumnWrapperRecordBatch> record_batch) {
  auto table = table_id_to_table_map_[table_id];
  PL_RETURN_IF_ERROR(table->TransferRecordBatch(std::move(record_batch)));
  return Status::OK();
}

// Temporary/Throwaway function
// TODO(oazizi/anyone): Remove once pub-sub with Stirling is fleshed out.
void TableStore::AddDefaultTable() {
  // Create RowDescriptor by copying schema from CPUStatBPFTraceConnector
  std::vector<udf::UDFDataType> col_types;
  std::vector<std::string> col_names;
  for (const auto& e : stirling::CPUStatBPFTraceConnector::kElements) {
    col_types.push_back(e.type());
    col_names.push_back(e.name());
  }
  plan::Relation relation(col_types, col_names);

  std::shared_ptr<exec::Table> table = std::make_shared<exec::Table>(relation);

  // Table_id: use any number (remember, this is throwaway code).
  uint64_t table_id = 314159;

  PL_CHECK_OK(AddTable(stirling::CPUStatBPFTraceConnector::kName, table_id, table));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
