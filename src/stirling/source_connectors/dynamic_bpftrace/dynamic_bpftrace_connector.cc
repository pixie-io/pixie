/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/dynamic_bpftrace/dynamic_bpftrace_connector.h"

#include <ast/async_event_types.h>

#include <utility>
#include <vector>

#include <absl/functional/bind_front.h>
#include <absl/memory/memory.h>
#include <absl/strings/ascii.h>

#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace stirling {

namespace {

StatusOr<types::DataType> BPFTraceTypeToDataType(const bpftrace::Type& bpftrace_type) {
  switch (bpftrace_type) {
    case bpftrace::Type::integer:
    case bpftrace::Type::pointer:
      return types::DataType::INT64;
    case bpftrace::Type::string:
    case bpftrace::Type::inet:
    case bpftrace::Type::usym:
    case bpftrace::Type::ksym:
    case bpftrace::Type::username:
    case bpftrace::Type::probe:
    case bpftrace::Type::kstack:
    case bpftrace::Type::ustack:
    case bpftrace::Type::timestamp:
      return types::DataType::STRING;
    default:
      return error::Internal("Unhandled field type $0.", magic_enum::enum_name(bpftrace_type));
  }
}

// Returns column names from the printf_fmt_str.
// The printf_fmt_str should be formatted as below:
// <column1_name>:%d <column2_name>:%s ...
//
// NOTE:
// * Column names must not have whitespaces.
// * Use ':' to separate column name and format specifier.
std::vector<std::string_view> GetColumnNamesFromFmtStr(std::string_view printf_fmt_str) {
  std::vector<std::string_view> res;

  std::vector<std::string_view> name_and_fmts =
      absl::StrSplit(printf_fmt_str, ' ', absl::SkipEmpty());
  for (auto name_and_fmt : name_and_fmts) {
    if (!absl::StrContains(name_and_fmt, "%")) {
      // Ignore non-formatting strings.
      continue;
    }

    std::vector<std::string_view> name_fmt_pair = absl::StrSplit(name_and_fmt, ':');

    std::string_view column_name;

    if (name_fmt_pair.size() >= 2) {
      column_name = absl::StripLeadingAsciiWhitespace(name_fmt_pair[0]);
    }
    res.push_back(column_name);
  }
  return res;
}

StatusOr<BackedDataElements> ConvertFields(const std::vector<bpftrace::Field> fields,
                                           std::string_view format_str) {
  BackedDataElements columns(fields.size());

  std::vector<std::string_view> column_names = GetColumnNamesFromFmtStr(format_str);

  if (column_names.size() != fields.size()) {
    return error::Internal("Column name count ($0) and actual output count ($1) do not match!",
                           column_names.size(), fields.size());
  }

  for (size_t i = 0; i < fields.size(); ++i) {
    bpftrace::Type bpftrace_type = fields[i].type.type;

    // Check: Any integers must be an expected size.
    if (bpftrace_type == bpftrace::Type::integer) {
      size_t bpftrace_type_size = fields[i].type.size();

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

    PX_ASSIGN_OR_RETURN(types::DataType col_type, BPFTraceTypeToDataType(bpftrace_type));
    std::string col_name =
        column_names[i].empty() ? absl::StrCat("Column_", i) : std::string(column_names[i]);
    // No way to set a description from BPFTrace code.
    std::string col_desc = "";

    // Special case adjustment for time.
    if (col_name == "time_") {
      if (col_type != types::DataType::INT64) {
        return error::Internal("time_ must be an integer type");
      }
      col_type = types::DataType::TIME64NS;
    }

    columns.emplace_back(std::move(col_name), std::move(col_desc), col_type);
  }

  return columns;
}

}  // namespace

StatusOr<std::unique_ptr<SourceConnector>> DynamicBPFTraceConnector::Create(
    std::string_view source_name,
    const dynamic_tracing::ir::logical::TracepointDeployment::Tracepoint& tracepoint) {
  auto bpftrace = std::make_unique<bpf_tools::BPFTraceWrapper>();
  PX_RETURN_IF_ERROR(bpftrace->CompileForPrintfOutput(tracepoint.bpftrace().program(), {}));
  const std::vector<bpftrace::Field>& fields = bpftrace->OutputFields();
  std::string_view format_str = bpftrace->OutputFmtStr();
  PX_ASSIGN_OR_RETURN(BackedDataElements columns, ConvertFields(fields, format_str));

  // Could consider making a better description, but may require more user input,
  // so punting on that for now.
  std::string desc = absl::StrCat("Dynamic table for ", tracepoint.table_name());

  std::unique_ptr<DynamicDataTableSchema> table_schema =
      DynamicDataTableSchema::Create(tracepoint.table_name(), desc, std::move(columns));

  return std::unique_ptr<SourceConnector>(
      new DynamicBPFTraceConnector(source_name, std::move(table_schema), std::move(bpftrace)));
}

DynamicBPFTraceConnector::DynamicBPFTraceConnector(
    std::string_view source_name, std::unique_ptr<DynamicDataTableSchema> table_schema,
    std::unique_ptr<bpf_tools::BPFTraceWrapper> bpftrace)
    : SourceConnector(source_name, ArrayView<DataTableSchema>(&table_schema->Get(), 1)),
      table_schema_(std::move(table_schema)),
      bpftrace_(std::move(bpftrace)) {}

namespace {

// Perform some checks on the fields to see that it is well formed.
// Important because we don't want the record builder to fail half-way through,
// otherwise the data table will be badly messed up.
Status CheckOutputFields(const std::vector<bpftrace::Field>& fields,
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

    PX_ASSIGN_OR_RETURN(types::DataType expected_type, BPFTraceTypeToDataType(bpftrace_type));

    if (table_schema_elements[i].name() == "time_") {
      expected_type = types::DataType::TIME64NS;
    }

    // Check #1: Type must be consistent with specified schema.
    if (table_type != expected_type) {
      return error::Internal(
          "Column $0, name=$1 (bpftrace type = $2) does not match expected output type ($3 vs $4).",
          i, table_schema_elements[i].name(), magic_enum::enum_name(bpftrace_type),
          magic_enum::enum_name(table_type), magic_enum::enum_name(expected_type));
    }

    // Check #2: Any integers must be an expected size.
    if (bpftrace_type == bpftrace::Type::integer) {
      size_t bpftrace_type_size = fields[i].type.size();

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
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  auto callback_fn = absl::bind_front(&DynamicBPFTraceConnector::HandleEvent, this);
  output_fields_ = bpftrace_->OutputFields();
  PX_RETURN_IF_ERROR(CheckOutputFields(output_fields_, table_schema_->Get().elements()));
  PX_RETURN_IF_ERROR(bpftrace_->Deploy(callback_fn));
  return Status::OK();
}

Status DynamicBPFTraceConnector::StopImpl() {
  bpftrace_->Stop();
  return Status::OK();
}

void DynamicBPFTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */) {
  DCHECK_EQ(data_tables_.size(), 1U) << "Only one table is allowed per DynamicBPFTraceConnector.";
  data_table_ = data_tables_[0];
  if (data_table_ == nullptr) {
    return;
  }
  // This trigger a callbacks for each BPFTrace printf event in the perf buffers.
  // Store data_table_ so the Handle function has the appropriate context.
  bpftrace_->PollPerfBuffers();
  data_table_ = nullptr;
}

namespace {

std::string ResolveInet(int af, const uint8_t* inet) {
  switch (af) {
    case AF_INET:
      return IPv4AddrToString(*reinterpret_cast<const struct in_addr*>(inet))
          .ValueOr("<Error decoding inet>");
    case AF_INET6:
      return IPv6AddrToString(*reinterpret_cast<const struct in6_addr*>(inet))
          .ValueOr("<Error decoding inet>");
      break;
  }

  // Shouldn't ever get here.
  return "<Error decoding inet>";
}

}  // namespace

void DynamicBPFTraceConnector::HandleEvent(uint8_t* data) {
  DataTable::DynamicRecordBuilder r(data_table_);

  const auto& columns = table_schema_->Get().elements();

  int col = 0;
  for (size_t i = 0; i < output_fields_.size(); ++i) {
    const auto& field = output_fields_[i];
    const auto& column = columns[i];

    switch (field.type.type) {
      case bpftrace::Type::integer:

#define APPEND_INTEGER(int_type, expr)                             \
  {                                                                \
    auto val = *reinterpret_cast<int_type*>(expr);                 \
    if (column.type() == types::DataType::TIME64NS) {              \
      r.Append(col, types::Time64NSValue(ConvertToRealTime(val))); \
    } else {                                                       \
      r.Append(col, types::Int64Value(val));                       \
    }                                                              \
  }

        switch (field.type.size()) {
          case 8:
            APPEND_INTEGER(uint64_t, data + field.offset);
            break;
          case 4:
            APPEND_INTEGER(uint32_t, data + field.offset);
            break;
          case 2:
            APPEND_INTEGER(uint16_t, data + field.offset);
            break;
          case 1:
            APPEND_INTEGER(uint8_t, data + field.offset);
            break;
          default:
            LOG(DFATAL) << absl::Substitute(
                "[DataTable: $0, col: $1] Invalid integer size: $2. Table is now inconsistent. "
                "This is a critical error.",
                name_, col, field.type.size());
            break;
        }
        break;
#undef APPEND_INTEGER
      case bpftrace::Type::string: {
        auto p = reinterpret_cast<char*>(data + field.offset);
        r.Append(col, types::StringValue(std::string(p, strnlen(p, field.type.size()))));
        break;
      }
      case bpftrace::Type::inet: {
        int64_t af = *reinterpret_cast<int64_t*>(data + field.offset);
        uint8_t* inet = reinterpret_cast<uint8_t*>(data + field.offset + 8);
        r.Append(col, types::StringValue(ResolveInet(af, inet)));
        break;
      }
      case bpftrace::Type::usym: {
        uint64_t addr = *reinterpret_cast<uint64_t*>(data + field.offset);
        uint64_t pid = *reinterpret_cast<uint64_t*>(data + field.offset + 8);
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->resolve_usym(addr, pid)));
        break;
      }
      case bpftrace::Type::ksym: {
        uint64_t addr = *reinterpret_cast<uint64_t*>(data + field.offset);
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->resolve_ksym(addr)));
        break;
      }
      case bpftrace::Type::username: {
        uint64_t addr = *reinterpret_cast<uint64_t*>(data + field.offset);
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->resolve_uid(addr)));
        break;
      }
      case bpftrace::Type::probe: {
        uint64_t probe_id = *reinterpret_cast<uint64_t*>(data + field.offset);
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->resolve_probe(probe_id)));
        break;
      }
      case bpftrace::Type::kstack: {
        uint64_t stackidpid = *reinterpret_cast<uint64_t*>(data + field.offset);
        bool ustack = false;
        auto& stack_type = field.type.stack_type;
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->get_stack(
                          stackidpid, ustack, stack_type)));
        break;
      }
      case bpftrace::Type::ustack: {
        uint64_t stackidpid = *reinterpret_cast<uint64_t*>(data + field.offset);
        bool ustack = true;
        auto& stack_type = field.type.stack_type;
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->get_stack(
                          stackidpid, ustack, stack_type)));
        break;
      }
      case bpftrace::Type::timestamp: {
        auto x = reinterpret_cast<bpftrace::AsyncEvent::Strftime*>(data + field.offset);
        r.Append(col, types::StringValue(bpftrace_->mutable_bpftrace()->resolve_timestamp(
                          x->strftime_id, x->nsecs_since_boot)));
        break;
      }
      case bpftrace::Type::pointer: {
        uint64_t p = *reinterpret_cast<uint64_t*>(data + field.offset);
        r.Append(col, types::Int64Value(p));
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
}  // namespace px
