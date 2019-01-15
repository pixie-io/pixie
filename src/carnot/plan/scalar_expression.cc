#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/utils/error.h"
#include "src/utils/statusor.h"

namespace pl {
namespace carnot {
namespace plan {

pl::Status ScalarValue::Init(const pl::carnot::planpb::ScalarValue &pb) {
  DCHECK(!is_initialized_) << "Already initialized";
  CHECK(pb.data_type() != planpb::DATA_TYPE_UNKNOWN);
  CHECK(planpb::DataType_IsValid(pb.data_type()));
  // TODO(zasgar): We should probably add a check to make sure that when a given
  // DataType is set, the wrong field value is not set.

  // Copy the proto.
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

int64_t ScalarValue::Int64Value() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != planpb::ScalarValue::kInt64Value)

      << "Calling accessor on null/invalid value";
  return pb_.int64_value();
}

double ScalarValue::Float64Value() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != planpb::ScalarValue::kFloat64Value)
      << "Calling accessor on null/invalid value";
  return pb_.float64_value();
}

std::string ScalarValue::StringValue() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != planpb::ScalarValue::kStringValue)
      << "Calling accessor on null/invalid value";
  return pb_.string_value();
}

bool ScalarValue::BoolValue() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != planpb::ScalarValue::kBoolValue)
      << "Calling accessor on null/invalid value";
  return pb_.bool_value();
}

bool ScalarValue::IsNull() const {
  DCHECK(is_initialized_) << "Not initialized";
  return pb_.value_case() == planpb::ScalarValue::VALUE_NOT_SET;
}

std::string ScalarValue::DebugString() const {
  DCHECK(is_initialized_) << "Not initialized";
  if (IsNull()) {
    return "<null>";
  }
  switch (DataType()) {
    case planpb::BOOLEAN:
      return BoolValue() ? "true" : "false";
    case planpb::INT64:
      return absl::StrFormat("%d", Int64Value());
    case planpb::FLOAT64:
      return absl::StrFormat("%ff", Float64Value());
    case planpb::STRING:
      return absl::StrFormat("\"%s\"", StringValue());
    default:
      return "<Unknown>";
  }
}

StatusOr<planpb::DataType> ScalarValue::OutputDataType(const Schema &input_schema) const {
  DCHECK(is_initialized_) << "Not initialized";
  PL_UNUSED(input_schema);
  return DataType();
}

std::vector<Column *> ScalarValue::ColumnDeps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

std::vector<ScalarExpression *> ScalarValue::Deps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

Status Column::Init(const planpb::Column &pb) {
  DCHECK(!is_initialized_) << "Already initialized";
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}
int64_t Column::Index() const {
  DCHECK(is_initialized_) << "Not initialized";
  return pb_.index();
}
int64_t Column::NodeID() const {
  DCHECK(is_initialized_) << "Not initialized";
  return pb_.node();
}
std::string Column::DebugString() const {
  DCHECK(is_initialized_) << "Not initialized";
  return absl::StrFormat("node<%d>::col[%d]", NodeID(), Index());
}

StatusOr<planpb::DataType> Column::OutputDataType(const Schema &input_schema) const {
  DCHECK(is_initialized_) << "Not initialized";
  StatusOr<const Relation> s = input_schema.GetRelation(NodeID());

  PL_RETURN_IF_ERROR(s);
  const auto &relation = s.ValueOrDie();
  planpb::DataType dt = relation.GetColumnType(Index());
  return dt;
}

std::vector<Column *> Column::ColumnDeps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {this};
}
std::vector<ScalarExpression *> Column::Deps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

template <typename T, typename TProto>
StatusOr<std::unique_ptr<ScalarExpression>> MakeExprHelper(const TProto &pb) {
  auto expr = std::make_unique<T>();
  auto s = expr->Init(pb);
  PL_RETURN_IF_ERROR(s);
  return std::unique_ptr<ScalarExpression>(std::move(expr));
}

StatusOr<std::unique_ptr<ScalarExpression>> ScalarExpression::FromProto(
    const planpb::ScalarExpression &pb) {
  switch (pb.value_case()) {
    case planpb::ScalarExpression::kColumn:
      return MakeExprHelper<Column>(pb.column());
    case planpb::ScalarExpression::kConstant:
      return MakeExprHelper<ScalarValue>(pb.constant());
    default:
      return error::Unimplemented("Expression type: %d", pb.value_case());
  }
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
