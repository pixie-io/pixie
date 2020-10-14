#include "src/stirling/dynamic_trace_connector.h"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "src/common/base/base.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

namespace pl {
namespace stirling {

using ::google::protobuf::RepeatedPtrField;

using ::pl::stirling::dynamic_tracing::ir::physical::Struct;
using ::pl::stirling::dynamic_tracing::ir::physical::StructSpec;
using ::pl::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::pl::utils::MemCpy;

namespace {

// A generic callback function to be invoked to push a piece of data polled from the perf buffer
// to the DynamicTraceConnector.
void GenericHandleEvent(void* cb_cookie, void* data, int data_size) {
  DCHECK_NE(cb_cookie, nullptr);
  DCHECK_EQ(data_size % 4, 0)
      << "Perf buffer data items are aligned with 8 bytes. "
         "The first 4 bytes are size, therefore data size must be a multiple of 4.";

  auto* parser = static_cast<DynamicTraceConnector*>(cb_cookie);
  std::string buf(static_cast<const char*>(data), data_size);

  parser->AcceptDataEvents(std::move(buf));
}

// A generic callback function to be invoked to process data item loss.
// The input cb_cookie has to be DynamicTraceConnector*.
void GenericHandleEventLoss(void* cb_cookie, uint64_t lost) {
  DCHECK_NE(cb_cookie, nullptr);
  VLOG(1) << absl::Substitute("Lost $0 events", lost);
}

}  // namespace

StatusOr<std::unique_ptr<SourceConnector>> DynamicTraceConnector::Create(
    std::string_view name, dynamic_tracing::ir::logical::TracepointDeployment* program) {
  PL_ASSIGN_OR_RETURN(dynamic_tracing::BCCProgram bcc_program,
                      dynamic_tracing::CompileProgram(program));

  LOG(INFO) << "BCCProgram:\n" << bcc_program.ToString();

  if (bcc_program.perf_buffer_specs.size() != 1) {
    return error::Internal("Only a single output table is allowed for now.");
  }

  const auto& output = bcc_program.perf_buffer_specs[0];

  return std::unique_ptr<SourceConnector>(new DynamicTraceConnector(
      name, DynamicDataTableSchema::Create(output), std::move(bcc_program)));
}

Status DynamicTraceConnector::InitImpl() {
  PL_RETURN_IF_ERROR(InitBPFProgram(bcc_program_.code));

  for (const auto& uprobe_spec : bcc_program_.uprobe_specs) {
    PL_RETURN_IF_ERROR(AttachUProbe(uprobe_spec));
  }

  // TODO(yzhao/oazizi): Might need to change this if we need to support multiple perf buffers.
  bpf_tools::PerfBufferSpec spec = {
      .name = bcc_program_.perf_buffer_specs.front().name,
      .probe_output_fn = &GenericHandleEvent,
      .probe_loss_fn = &GenericHandleEventLoss,
  };

  PL_RETURN_IF_ERROR(OpenPerfBuffer(spec, this));

  return Status::OK();
}

namespace {

// Reads a byte sequence representing a packed C/C++ struct, and extract the values of the fields.
class StructDecoder {
 public:
  explicit StructDecoder(std::string_view buf) : buf_(buf) {}

  template <typename NativeScalarType>
  StatusOr<NativeScalarType> ExtractField() {
    if (buf_.size() < sizeof(NativeScalarType)) {
      return error::ResourceUnavailable("Insufficient number of bytes.");
    }
    auto val = MemCpy<NativeScalarType>(buf_);
    buf_.remove_prefix(sizeof(NativeScalarType));
    return val;
  }

  StatusOr<std::string> ExtractString() {
    // NOTE: This implementation must match "struct string" defined in code_gen.cc.
    // A copy is provided here for reference:
    //
    // #define MAX_STR_LEN (kStructStringSize-sizeof(int64_t)-1)
    // struct blob {
    //   uint64_t len;
    //   char buf[MAX_STR_LEN];
    //   // To keep 4.14 kernel verifier happy we copy an extra byte.
    //   // Keep a dummy character to absorb this garbage.
    //   // We also use this extra byte to track if data has been truncated.
    //   uint8_t dummy;
    // };
    //
    // TODO(oazizi): Find a better way to keep these in sync.
    PL_ASSIGN_OR_RETURN(size_t len, ExtractField<size_t>());
    std::string s(buf_.substr(0, len));
    buf_.remove_prefix(dynamic_tracing::kStructStringSize - sizeof(size_t) - 1);
    PL_ASSIGN_OR_RETURN(uint8_t truncated, ExtractField<uint8_t>());

    if (truncated) {
      absl::StrAppend(&s, "<truncated>");
    }
    return s;
  }

  StatusOr<std::string> ExtractByteArrayAsHex() {
    // NOTE: This implementation must match "struct byte_array" defined in code_gen.cc.
    // A copy is provided here for reference:
    //
    // #define MAX_BYTE_ARRAY_LEN (kStructStringSize-sizeof(int64_t)-1)
    // struct byte_array {
    //   uint64_t len;
    //   uint8_t buf[MAX_BYTE_ARRAY_LEN];
    //   // To keep 4.14 kernel verifier happy we copy an extra byte.
    //   // Keep a dummy character to absorb this garbage.
    //   // We also use this extra byte to track if data has been truncated.
    //   uint8_t dummy;
    // };
    //
    // TODO(oazizi): Find a better way to keep these in sync.
    PL_ASSIGN_OR_RETURN(size_t len, ExtractField<size_t>());

    std::string_view bytes = buf_.substr(0, len);

    buf_.remove_prefix(dynamic_tracing::kStructByteArraySize - sizeof(size_t) - 1);
    PL_ASSIGN_OR_RETURN(uint8_t truncated, ExtractField<uint8_t>());

    std::string s = BytesToString<bytes_format::HexCompact>(bytes);
    if (truncated) {
      absl::StrAppend(&s, "<truncated>");
    }
    return s;
  }

  StatusOr<std::string> ExtractStructBlobAsJSON(const StructSpec& col_decoder) {
    PL_ASSIGN_OR_RETURN(size_t len, ExtractField<size_t>());
    std::string_view bytes = buf_.substr(0, len);
    buf_.remove_prefix(dynamic_tracing::kStructBlobSize - sizeof(size_t));

    rapidjson::Document d;
    d.SetObject();
    for (const auto& entry : col_decoder.entries()) {
      const char* ptr = bytes.data() + entry.offset();

#define CASE(type)                                       \
  {                                                      \
    type p2 = MemCpy<type>(ptr);                         \
    rapidjson::Pointer(entry.path().c_str()).Set(d, p2); \
    break;                                               \
  }

      switch (entry.type()) {
        case ScalarType::BOOL:
          CASE(bool);
        case ScalarType::INT:
          CASE(int);
        case ScalarType::INT8:
          CASE(int8_t);
        case ScalarType::INT16:
          CASE(int16_t);
        case ScalarType::INT32:
          CASE(int32_t);
        case ScalarType::INT64:
          CASE(int64_t);
        case ScalarType::UINT:
          CASE(unsigned int);
        case ScalarType::UINT8:
          CASE(uint8_t);
        case ScalarType::UINT16:
          CASE(uint16_t);
        case ScalarType::UINT32:
          CASE(uint32_t);
        case ScalarType::UINT64:
          CASE(uint64_t);
        case ScalarType::SHORT:
          // NOLINTNEXTLINE(runtime/int)
          CASE(short);
        case ScalarType::USHORT:
          // NOLINTNEXTLINE(runtime/int)
          CASE(unsigned short);
        case ScalarType::LONG:
          // NOLINTNEXTLINE(runtime/int)
          CASE(long);
        case ScalarType::ULONG:
          // NOLINTNEXTLINE(runtime/int)
          CASE(unsigned long);
        case ScalarType::LONGLONG:
          // NOLINTNEXTLINE(runtime/int)
          CASE(int64_t);  // NOTE: had to change from "long long" for rapidjson
        case ScalarType::ULONGLONG:
          // NOLINTNEXTLINE(runtime/int)
          CASE(uint64_t);  // NOTE: had to change from "unsigned long long" for rapidjson
        case ScalarType::CHAR:
          CASE(char);
        case ScalarType::UCHAR:
          CASE(unsigned char);
        case ScalarType::FLOAT:
          CASE(float);
        case ScalarType::DOUBLE:
          CASE(double);
        case ScalarType::VOID_POINTER:
          CASE(uint64_t);
        default:
          LOG(DFATAL) << absl::Substitute("Unhandled type=$0", entry.type());
      }
    }

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    d.Accept(writer);
    return std::string(sb.GetString());
  }

 private:
  std::string_view buf_;
};

Status FillColumn(StructDecoder* struct_decoder, DataTable::DynamicRecordBuilder* r, size_t col_idx,
                  ScalarType type, const RepeatedPtrField<StructSpec>& col_decoder) {
#define WRITE_COLUMN(field_type, column_type)                                        \
  {                                                                                  \
    PL_ASSIGN_OR_RETURN(field_type val, struct_decoder->ExtractField<field_type>()); \
    r->Append(col_idx, column_type(val));                                            \
    break;                                                                           \
  }

  // TODO(yzhao): Right now only support scalar types. We should replace type with ScalarType
  // in Struct::Field.
  switch (type) {
    case ScalarType::BOOL:
      WRITE_COLUMN(bool, types::BoolValue);
    case ScalarType::INT:
      WRITE_COLUMN(int, types::Int64Value);
    case ScalarType::INT8:
      WRITE_COLUMN(int8_t, types::Int64Value);
    case ScalarType::INT16:
      WRITE_COLUMN(int16_t, types::Int64Value);
    case ScalarType::INT32:
      WRITE_COLUMN(int32_t, types::Int64Value);
    case ScalarType::INT64:
      WRITE_COLUMN(int64_t, types::Int64Value);
    case ScalarType::UINT:
      WRITE_COLUMN(unsigned int, types::Int64Value);
    case ScalarType::UINT8:
      WRITE_COLUMN(uint8_t, types::Int64Value);
    case ScalarType::UINT16:
      WRITE_COLUMN(uint16_t, types::Int64Value);
    case ScalarType::UINT32:
      WRITE_COLUMN(uint32_t, types::Int64Value);
    case ScalarType::UINT64:
      WRITE_COLUMN(uint64_t, types::Int64Value);

    case ScalarType::SHORT:
      // NOLINTNEXTLINE(runtime/int)
      WRITE_COLUMN(short, types::Int64Value);
    case ScalarType::USHORT:
      // NOLINTNEXTLINE(runtime/int)
      WRITE_COLUMN(unsigned short, types::Int64Value);
    case ScalarType::LONG:
      // NOLINTNEXTLINE(runtime/int)
      WRITE_COLUMN(long, types::Int64Value);
    case ScalarType::ULONG:
      // NOLINTNEXTLINE(runtime/int)
      WRITE_COLUMN(unsigned long, types::Int64Value);
    case ScalarType::LONGLONG:
      // NOLINTNEXTLINE(runtime/int)
      WRITE_COLUMN(long long, types::Int64Value);
    case ScalarType::ULONGLONG:
      // NOLINTNEXTLINE(runtime/int)
      WRITE_COLUMN(unsigned long long, types::Int64Value);
    case ScalarType::CHAR:
      WRITE_COLUMN(char, types::Int64Value);
    case ScalarType::UCHAR:
      WRITE_COLUMN(unsigned char, types::Int64Value);

    case ScalarType::FLOAT:
      WRITE_COLUMN(float, types::Float64Value);
    case ScalarType::DOUBLE:
      WRITE_COLUMN(double, types::Float64Value);
    case ScalarType::VOID_POINTER:
      WRITE_COLUMN(uint64_t, types::Int64Value);
    case ScalarType::STRING: {
      PL_ASSIGN_OR_RETURN(std::string val, struct_decoder->ExtractString());
      r->Append(col_idx, types::StringValue(val));
      break;
    }
    case ScalarType::BYTE_ARRAY: {
      PL_ASSIGN_OR_RETURN(std::string val, struct_decoder->ExtractByteArrayAsHex());
      r->Append(col_idx, types::StringValue(val));
      break;
    }
    case ScalarType::STRUCT_BLOB: {
      ECHECK(col_decoder.size() == 1) << "Only support exactly one StructSpec for StructBlob";
      PL_ASSIGN_OR_RETURN(std::string val,
                          struct_decoder->ExtractStructBlobAsJSON(col_decoder.Get(0)));
      r->Append(col_idx, types::StringValue(val));
      break;
    }
    case ScalarType::UNKNOWN:
      return error::Internal("Unknown scalar type should not be used.");
    case ScalarType::ScalarType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ScalarType::ScalarType_INT_MAX_SENTINEL_DO_NOT_USE_:
      LOG(DFATAL) << "Impossible enum value";
      break;
  }
#undef WRITE_COLUMN

  return Status::OK();
}

}  // namespace

Status DynamicTraceConnector::AppendRecord(const Struct& st, uint32_t asid, std::string_view buf,
                                           DataTable* data_table) {
  StructDecoder struct_decoder(buf);
  DataTable::DynamicRecordBuilder r(data_table);

  int col_idx = 0;
  for (int i = 0; i < st.fields_size(); ++i) {
    auto& field = st.fields(i);

    if (field.name() == "time_") {
      PL_ASSIGN_OR_RETURN(uint64_t ktime_ns, struct_decoder.ExtractField<uint64_t>());
      int64_t time = ktime_ns + ClockRealTimeOffset();
      r.Append(col_idx++, types::Time64NSValue(time));
    } else if ((field.name() == "tgid_") && (i + 1 < st.fields_size()) &&
               (st.fields(i + 1).name() == "tgid_start_time_")) {
      // If we see "tgid_" and "tgid_start_time_" back-to-back, then we automatically create UPID.
      PL_ASSIGN_OR_RETURN(uint32_t tgid, struct_decoder.ExtractField<uint32_t>());
      PL_ASSIGN_OR_RETURN(uint64_t tgid_start_time, struct_decoder.ExtractField<uint64_t>());
      md::UPID upid(asid, tgid, tgid_start_time);
      r.Append(col_idx++, types::UInt128Value(upid.value()));

      // Consume the extra tgid_start_time_ column.
      ++i;
    } else {
      PL_RETURN_IF_ERROR(
          FillColumn(&struct_decoder, &r, col_idx++, field.type(), field.blob_decoder()));
    }
  }

  return Status::OK();
}

void DynamicTraceConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                             DataTable* data_table) {
  DCHECK_EQ(table_num, 0) << "Now only support having exactly one table per DynamicTraceConnector";

  PollPerfBuffers();

  for (const auto& item : data_items_) {
    // TODO(yzhao): Right now only support scalar types. We should replace type with ScalarType
    // in Struct::Field.
    ECHECK_OK(AppendRecord(bcc_program_.perf_buffer_specs.front().output, ctx->GetASID(), item,
                           data_table));
  }

  data_items_.clear();
}

}  // namespace stirling
}  // namespace pl
