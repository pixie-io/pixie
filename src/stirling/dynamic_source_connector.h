#pragma once

#include <memory>
#include <random>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

class DynamicSourceConnector : public SourceConnector {
 public:
  ~DynamicSourceConnector() override = default;

  static std::unique_ptr<SourceConnector> Create(
      std::string_view name, std::unique_ptr<DynamicDataTableSchema> table_schema) {
    return std::unique_ptr<SourceConnector>(
        new DynamicSourceConnector(name, std::move(table_schema)));
  }

 protected:
  // TODO(oazizi): This constructor only works with a single table,
  //               since the ArrayView creation only works for a single schema.
  //               Consider how to expand to multiple tables if/when needed.
  DynamicSourceConnector(std::string_view name,
                         std::unique_ptr<DynamicDataTableSchema> table_schema)
      : SourceConnector(name, ArrayView<DataTableSchema>(&table_schema->Get(), 1)),
        table_schema_(std::move(table_schema)),
        coin_flip_dist_(0, 1) {}

  Status InitImpl() override { return Status::OK(); }

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

  Status StopImpl() override { return Status::OK(); }

 private:
  std::unique_ptr<DynamicDataTableSchema> table_schema_;

  // TODO(oazizi): Temporary remove.
  std::default_random_engine rng_;
  std::uniform_int_distribution<int> coin_flip_dist_;
};

}  // namespace stirling
}  // namespace pl
