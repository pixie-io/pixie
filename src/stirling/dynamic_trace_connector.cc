#include "src/stirling/dynamic_trace_connector.h"
#include "src/shared/types/proto/types.pb.h"

namespace pl {
namespace stirling {

Status DynamicTraceConnector::InitImpl() {
  PL_UNUSED(bcc_program_);
  // Run DeployBCCProgram(bcc_program) here.
  return Status::OK();
}

void DynamicTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                             DataTable* data_table) {
  const DataTableSchema& table_schema = TableSchema(table_num);

  DataTable::DynamicRecordBuilder r(data_table);

  for (size_t i = 0; i < table_schema.elements().size(); ++i) {
    const auto& element = table_schema.elements()[i];

    // TODO(oazizi): Currently output random data. Needs to be updated.
    switch (element.type()) {
      case types::DataType::INT64:
        r.Append(i, types::Int64Value(42));
        break;
      case types::DataType::BOOLEAN:
        r.Append(i, types::BoolValue(coin_flip_dist_(rng_)));
        break;
      case types::DataType::FLOAT64:
        r.Append(i, types::Float64Value(3.14159));
        break;
      case types::DataType::STRING:
        r.Append(i, types::StringValue("pixie!"));
        break;
      case types::DataType::TIME64NS:
        r.Append(i,
                 types::Time64NSValue(std::chrono::system_clock::now().time_since_epoch().count()));
        break;
      case types::DataType::UINT128:
        r.Append(i, types::UInt128Value(0x123456789abcdef, 0xfdecba9876543210));
        break;
      default:
        break;
    }
  }
}

}  // namespace stirling
}  // namespace pl
