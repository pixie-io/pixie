#include "src/stirling/dynamic_tracing/code_gen.h"

#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dynamictracingpb::ScalarType;
using ::pl::stirling::dynamictracingpb::Struct;
using ::pl::stirling::dynamictracingpb::ValueType;

namespace {

StatusOr<std::string> GenScalarField(const Struct::Field& field) {
  switch (field.type().scalar()) {
    case ScalarType::INT32:
      return absl::Substitute("int32_t $0;", field.name());
    case ScalarType::INT64:
      return absl::Substitute("int64_t $0;", field.name());
    case ScalarType::DOUBLE:
      return absl::Substitute("double $0;", field.name());
    case ScalarType::STRING:
      return absl::Substitute("char* $0;", field.name());
    case ScalarType::VOID_POINTER:
      return absl::Substitute("void* $0;", field.name());
    case ScalarType::ScalarType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ScalarType::ScalarType_INT_MAX_SENTINEL_DO_NOT_USE_:
      DCHECK("Needed to avoid default clause");
      return {};
  }
  return error::InvalidArgument("Should never happen");
}

StatusOr<std::string> GenField(const Struct::Field& field) {
  switch (field.type().type_oneof_case()) {
    case ValueType::TypeOneofCase::kScalar:
      return GenScalarField(field);
    case ValueType::TypeOneofCase::kStructType:
      return absl::Substitute("struct $0 $1;", field.type().struct_type(), field.name());
    case ValueType::TypeOneofCase::TYPE_ONEOF_NOT_SET:
      return error::InvalidArgument("Field type must be set");
  }
  return error::InvalidArgument("Should never happen");
}

}  // namespace

StatusOr<std::string> GenStruct(const Struct& st, int indent_size) {
  DCHECK_GT(st.fields_size(), 0);

  std::string bcc_code;

  absl::StrAppend(&bcc_code, absl::Substitute("struct $0 {\n", st.name()));

  for (const auto& field : st.fields()) {
    PL_ASSIGN_OR_RETURN(std::string field_code, GenField(field));

    absl::StrAppend(&bcc_code, std::string(indent_size, ' '), field_code, "\n");
  }

  absl::StrAppend(&bcc_code, "};\n");

  return bcc_code;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
