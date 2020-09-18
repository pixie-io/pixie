#ifdef __linux__

#include "src/stirling/dynamic_bpftrace_connector.h"
#include "src/shared/types/proto/types.pb.h"

#include <utility>
#include <vector>

#include <absl/memory/memory.h>

namespace pl {
namespace stirling {

std::unique_ptr<SourceConnector> DynamicBPFTraceConnector::Create(
    std::string_view source_name,
    const dynamic_tracing::ir::logical::TracepointDeployment::Tracepoint& tracepoint) {
  std::unique_ptr<DynamicDataTableSchema> table_schema =
      DynamicDataTableSchema::Create(tracepoint.table_name(), tracepoint.bpftrace());
  return std::unique_ptr<SourceConnector>(new DynamicBPFTraceConnector(
      source_name, std::move(table_schema), tracepoint.bpftrace().program()));
}

DynamicBPFTraceConnector::DynamicBPFTraceConnector(
    std::string_view source_name, std::unique_ptr<DynamicDataTableSchema> table_schema,
    std::string_view script)
    : SourceConnector(source_name, ArrayView<DataTableSchema>(&table_schema->Get(), 1)),
      table_schema_(std::move(table_schema)),
      script_(script) {}

namespace {

// Perform some checks on the fields to see that it is well formed.
// Important because we don't want the record builder to fail half-way through,
// otherwise the data table will be badly messed up.
Status CheckOutputFields(const std::vector<bpftrace::Field> fields,
                         const ArrayView<DataElement>& table_schema_elements) {
  if (fields.size() != table_schema_elements.size()) {
    return error::Internal(
        "Number of fields from BPFTrace ($0) does not match number of fields from specified schema "
        "($1).",
        fields.size(), table_schema_elements.size());
  }

  for (size_t i = 0; i < fields.size(); ++i) {
    bpftrace::Type bpftrace_type = fields[i].type.type;
    types::DataType table_type = table_schema_elements[i].type();

    types::DataType expected_type = types::DataType::DATA_TYPE_UNKNOWN;
    switch (bpftrace_type) {
      case bpftrace::Type::integer:
        expected_type = types::DataType::INT64;
        break;
      case bpftrace::Type::string:
        expected_type = types::DataType::STRING;
        break;
      default:
        return error::Internal("Column $0 has an unhandled field type $1.", i,
                               magic_enum::enum_name(bpftrace_type));
    }

    // Check #1: Type must be consistent with specified schema.
    if (table_type != expected_type) {
      return error::Internal("Column $0 does not match expected output type ($1 vs $2).", i,
                             magic_enum::enum_name(bpftrace_type),
                             magic_enum::enum_name(table_type));
    }

    // Check #2: Any integers must be an expected size.
    if (bpftrace_type == bpftrace::Type::integer) {
      size_t bpftrace_type_size = fields[i].type.size;

      switch (bpftrace_type_size) {
        case 8:
        case 4:
        case 2:
        case 1:
          break;
        default:
          return error::Internal("Perf event on column $0 contains invalid integer size: $1.", i,
                                 bpftrace_type_size);
      }
    }
  }

  return Status::OK();
}

}  // namespace

Status DynamicBPFTraceConnector::InitImpl() {
  auto callback_fn = std::bind(&DynamicBPFTraceConnector::HandleEvent, this, std::placeholders::_1);
  PL_RETURN_IF_ERROR(Deploy(script_, {}, callback_fn));
  PL_ASSIGN_OR_RETURN(output_fields_, OutputFields());
  PL_RETURN_IF_ERROR(CheckOutputFields(output_fields_, table_schema_->Get().elements()));
  return Status::OK();
}

Status DynamicBPFTraceConnector::StopImpl() {
  BPFTraceWrapper::Stop();
  return Status::OK();
}

void DynamicBPFTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                                DataTable* data_table) {
  DCHECK_EQ(table_num, 0) << "Only one table is allowed per DynamicBPFTraceConnector.";

  // This trigger a callbacks for each BPFTrace printf event in the perf buffers.
  // Store data_table_ so the Handle function has the appropriate context.
  data_table_ = data_table;
  PollPerfBuffers();
  data_table_ = nullptr;
}

void DynamicBPFTraceConnector::HandleEvent(uint8_t* data) {
  DataTable::DynamicRecordBuilder r(data_table_);

  int col = 0;
  for (const auto& field : output_fields_) {
    switch (field.type.type) {
      case bpftrace::Type::integer:
        switch (field.type.size) {
          case 8:
            r.Append(col, types::Int64Value(*reinterpret_cast<uint64_t*>(data + field.offset)));
            break;
          case 4:
            r.Append(col, types::Int64Value(*reinterpret_cast<uint32_t*>(data + field.offset)));
            break;
          case 2:
            r.Append(col, types::Int64Value(*reinterpret_cast<uint16_t*>(data + field.offset)));
            break;
          case 1:
            r.Append(col, types::Int64Value(*reinterpret_cast<uint8_t*>(data + field.offset)));
            break;
          default:
            LOG(DFATAL) << absl::Substitute(
                "[DataTable: $0, col: $1] Invalid integer size: $2. Table is now inconsistent. "
                "This is a critical error.",
                name_, col, field.type.size);
            break;
        }
        break;
      case bpftrace::Type::string: {
        auto p = reinterpret_cast<char*>(data + field.offset);
        r.Append(col, types::StringValue(std::string(p, strnlen(p, field.type.size))));
        break;
      }
      default:
        LOG(DFATAL) << absl::Substitute(
            "[DataTable: $0, col: $1] Invalid argument type $2. Table is now inconsistent. This is "
            "a critical error.",
            name_, col, magic_enum::enum_name(field.type.type));
    }

    ++col;
  }
}

}  // namespace stirling
}  // namespace pl

#endif
