#include "src/stirling/output.h"

namespace pl {
namespace stirling {

using pl::types::ColumnWrapperRecordBatch;
using pl::types::DataType;
using pl::types::Duration64NSValue;
using pl::types::Float64Value;
using pl::types::Int64Value;
using pl::types::StringValue;
using pl::types::Time64NSValue;
using pl::types::UInt128Value;

void PrintRecordBatch(std::string_view prefix, const ArrayView<DataElement>& schema,
                      size_t num_records, const ColumnWrapperRecordBatch& record_batch) {
  constexpr char kTimeFormat[] = "%Y-%m-%d %X";
  static absl::TimeZone tz;

  for (size_t i = 0; i < num_records; ++i) {
    std::cout << "[" << prefix << "]";

    uint32_t j = 0;
    for (const auto& col : record_batch) {
      std::cout << " " << schema[j].name() << ":";
      switch (schema[j].type()) {
        case DataType::TIME64NS: {
          const auto val = col->Get<Time64NSValue>(i).val;
          std::time_t time = val / 1000000000UL;
          absl::Time t = absl::FromTimeT(time);
          std::cout << "[" << absl::FormatTime(kTimeFormat, t, tz) << "]";
        } break;
        case DataType::INT64: {
          const auto val = col->Get<Int64Value>(i).val;
          std::cout << "[" << val << "]";
        } break;
        case DataType::FLOAT64: {
          const auto val = col->Get<Float64Value>(i).val;
          std::cout << "[" << val << "]";
        } break;
        case DataType::STRING: {
          const auto& val = col->Get<StringValue>(i);
          std::cout << "[" << val << "]";
        } break;
        case DataType::UINT128: {
          const auto& val = col->Get<UInt128Value>(i);
          std::cout << "[" << absl::Substitute("{$0,$1}", val.High64(), val.Low64()) << "]";
        } break;
        case DataType::DURATION64NS: {
          const auto secs = std::chrono::duration_cast<std::chrono::duration<double>>(
              std::chrono::nanoseconds(col->Get<Duration64NSValue>(i).val));
          std::cout << absl::Substitute("[$0 seconds]", secs.count());
        } break;
        default:
          LOG(DFATAL) << absl::Substitute("Unrecognized type: $0", ToString(schema[j].type()));
      }
      j++;
    }
    std::cout << std::endl;
  }
}

}  // namespace stirling
}  // namespace pl
