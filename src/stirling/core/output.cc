#include "src/stirling/core/output.h"

#include "src/shared/upid/upid.h"

namespace pl {
namespace stirling {

using pl::types::BoolValue;
using pl::types::ColumnWrapperRecordBatch;
using pl::types::DataType;
using pl::types::Float64Value;
using pl::types::Int64Value;
using pl::types::SemanticType;
using pl::types::StringValue;
using pl::types::Time64NSValue;
using pl::types::UInt128Value;

constexpr char kTimeFormat[] = "%Y-%m-%d %X";
const absl::TimeZone kLocalTimeZone;

std::string ToString(const stirlingpb::TableSchema& schema,
                     const ColumnWrapperRecordBatch& record_batch, size_t index) {
  DCHECK(!record_batch.empty());
  DCHECK_EQ(schema.elements_size(), record_batch.size());
  DCHECK_LT(index, record_batch[0]->Size());

  std::string out;
  for (int j = 0; j < schema.elements_size(); ++j) {
    const auto& col = record_batch[j];
    const auto& col_schema = schema.elements(j);

    absl::StrAppend(&out, " ", col_schema.name(), ":[");

    switch (col_schema.type()) {
      case DataType::TIME64NS: {
        const auto val = col->Get<Time64NSValue>(index).val;
        std::time_t time = val / 1000000000UL;
        absl::Time t = absl::FromTimeT(time);
        absl::StrAppend(&out, absl::FormatTime(kTimeFormat, t, kLocalTimeZone));
      } break;
      case DataType::INT64: {
        const auto val = col->Get<Int64Value>(index).val;
        if (col_schema.stype() == SemanticType::ST_DURATION_NS) {
          const auto secs = std::chrono::duration_cast<std::chrono::duration<double>>(
              std::chrono::nanoseconds(val));
          absl::StrAppend(&out, absl::Substitute("$0 seconds", secs.count()));
        } else {
          absl::StrAppend(&out, val);
        }
      } break;
      case DataType::FLOAT64: {
        const auto val = col->Get<Float64Value>(index).val;
        absl::StrAppend(&out, val);
      } break;
      case DataType::BOOLEAN: {
        const auto val = col->Get<BoolValue>(index).val;
        absl::StrAppend(&out, val);
      } break;
      case DataType::STRING: {
        const auto& val = col->Get<StringValue>(index);
        absl::StrAppend(&out, val);
      } break;
      case DataType::UINT128: {
        const auto& val = col->Get<UInt128Value>(index);
        if (col_schema.stype() == SemanticType::ST_UPID) {
          md::UPID upid(val.val);
          absl::StrAppend(&out, absl::Substitute("{$0}", upid.String()));
        } else {
          absl::StrAppend(&out, absl::Substitute("{$0,$1}", val.High64(), val.Low64()));
        }
      } break;
      default:
        LOG(DFATAL) << absl::Substitute("Unrecognized type: $0", ToString(col_schema.type()));
    }

    absl::StrAppend(&out, "]");
  }
  return out;
}

std::string ToString(std::string_view prefix, const stirlingpb::TableSchema& schema,
                     const types::ColumnWrapperRecordBatch& record_batch) {
  DCHECK_EQ(schema.elements_size(), record_batch.size());

  const size_t num_records = record_batch.front()->Size();

  for (const auto& col : record_batch) {
    DCHECK_EQ(col->Size(), num_records);
  }

  std::string out;
  for (size_t i = 0; i < num_records; ++i) {
    absl::StrAppend(&out, "[", prefix, "]", ToString(schema, record_batch, i), "\n");
  }
  return out;
}

void PrintRecordBatch(std::string_view prefix, const stirlingpb::TableSchema& schema,
                      const ColumnWrapperRecordBatch& record_batch) {
  std::cout << ToString(prefix, schema, record_batch);
}

}  // namespace stirling
}  // namespace pl
