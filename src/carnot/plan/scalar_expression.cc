#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "src/carnot/plan/compiler_state.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/plan/utils.h"
#include "src/carnot/udf/udf.h"
#include "src/utils/error.h"
#include "src/utils/statusor.h"

namespace pl {
namespace carnot {
namespace plan {

pl::Status ScalarValue::Init(const pl::carnot::carnotpb::ScalarValue &pb) {
  DCHECK(!is_initialized_) << "Already initialized";
  CHECK(pb.data_type() != carnotpb::DATA_TYPE_UNKNOWN);
  CHECK(carnotpb::DataType_IsValid(pb.data_type()));
  // TODO(zasgar): We should probably add a check to make sure that when a given
  // DataType is set, the wrong field value is not set.

  // Copy the proto.
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

int64_t ScalarValue::Int64Value() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != carnotpb::ScalarValue::kInt64Value)

      << "Calling accessor on null/invalid value";
  return pb_.int64_value();
}

double ScalarValue::Float64Value() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != carnotpb::ScalarValue::kFloat64Value)
      << "Calling accessor on null/invalid value";
  return pb_.float64_value();
}

std::string ScalarValue::StringValue() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != carnotpb::ScalarValue::kStringValue)
      << "Calling accessor on null/invalid value";
  return pb_.string_value();
}

bool ScalarValue::BoolValue() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != carnotpb::ScalarValue::kBoolValue)
      << "Calling accessor on null/invalid value";
  return pb_.bool_value();
}

bool ScalarValue::IsNull() const {
  DCHECK(is_initialized_) << "Not initialized";
  return pb_.value_case() == carnotpb::ScalarValue::VALUE_NOT_SET;
}

std::string ScalarValue::DebugString() const {
  DCHECK(is_initialized_) << "Not initialized";
  if (IsNull()) {
    return "<null>";
  }
  switch (DataType()) {
    case carnotpb::BOOLEAN:
      return BoolValue() ? "true" : "false";
    case carnotpb::INT64:
      return absl::StrFormat("%d", Int64Value());
    case carnotpb::FLOAT64:
      return absl::StrFormat("%ff", Float64Value());
    case carnotpb::STRING:
      return absl::StrFormat("\"%s\"", StringValue());
    default:
      return "<Unknown>";
  }
}

StatusOr<carnotpb::DataType> ScalarValue::OutputDataType(const CompilerState &,
                                                         const Schema &) const {
  DCHECK(is_initialized_) << "Not initialized";
  return DataType();
}

std::vector<const Column *> ScalarValue::ColumnDeps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

std::vector<ScalarExpression *> ScalarValue::Deps() const {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}
carnotpb::ScalarExpression::ValueCase ScalarValue::ExpressionType() const {
  return carnotpb::ScalarExpression::kConstant;
}

Status Column::Init(const carnotpb::Column &pb) {
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

StatusOr<carnotpb::DataType> Column::OutputDataType(const CompilerState &,
                                                    const Schema &input_schema) const {
  DCHECK(is_initialized_) << "Not initialized";
  StatusOr<const Relation> s = input_schema.GetRelation(NodeID());

  PL_RETURN_IF_ERROR(s);
  const auto &relation = s.ValueOrDie();
  carnotpb::DataType dt = relation.GetColumnType(Index());
  return dt;
}

std::vector<const Column *> Column::ColumnDeps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {this};
}

carnotpb::ScalarExpression::ValueCase Column::ExpressionType() const {
  return carnotpb::ScalarExpression::kColumn;
}

std::vector<ScalarExpression *> Column::Deps() const {
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
    const carnotpb::ScalarExpression &pb) {
  switch (pb.value_case()) {
    case carnotpb::ScalarExpression::kColumn:
      return MakeExprHelper<Column>(pb.column());
    case carnotpb::ScalarExpression::kConstant:
      return MakeExprHelper<ScalarValue>(pb.constant());
    case carnotpb::ScalarExpression::kFunc:
      return MakeExprHelper<ScalarFunc>(pb.func());
    default:
      return error::Unimplemented("Expression type: %d", pb.value_case());
  }
}

Status ScalarFunc::Init(const carnotpb::ScalarFunc &pb) {
  name_ = pb.name();
  for (const auto arg : pb.args()) {
    auto s = ScalarExpression::FromProto(arg);
    if (!s.ok()) {
      return s.status();
    }
    arg_deps_.emplace_back(s.ConsumeValueOrDie());
  }
  return Status::OK();
}

std::vector<ScalarExpression *> ScalarFunc::Deps() const {
  std::vector<ScalarExpression *> deps;
  for (const auto &arg : arg_deps_) {
    // No ownership transfer.
    deps.emplace_back(arg.get());
  }
  return deps;
}

carnotpb::ScalarExpression::ValueCase ScalarFunc::ExpressionType() const {
  return carnotpb::ScalarExpression::kFunc;
}

std::vector<const Column *> ScalarFunc::ColumnDeps() {
  std::vector<const Column *> cols;
  ScalarExpressionWalker<int>()
      .OnColumn([&](const auto &col, const auto &) {
        cols.push_back(&col);
        return 0;
      })
      .Walk(*this);
  return cols;
}

StatusOr<carnotpb::DataType> ScalarFunc::OutputDataType(const CompilerState &state,
                                                        const Schema &input_schema) const {
  // The output data type of a function is based on the computed types of the children
  // followed by the looking up the function in the registry and getting the output
  // data type of the function.
  auto res =
      ScalarExpressionWalker<StatusOr<carnotpb::DataType>>()
          .OnScalarValue([&](auto &val, auto &) -> StatusOr<carnotpb::DataType> {
            return val.OutputDataType(state, input_schema);
          })
          .OnColumn([&](auto &col, auto &) -> StatusOr<carnotpb::DataType> {
            return col.OutputDataType(state, input_schema);
          })
          .OnScalarFunc([&](auto &func, auto &child_results) -> StatusOr<carnotpb::DataType> {
            std::vector<carnotpb::DataType> child_args;
            child_args.reserve(child_results.size());
            for (const auto &child_result : child_results) {
              PL_RETURN_IF_ERROR(child_result);
              child_args.push_back(child_result.ValueOrDie());
            }
            auto s = state.udf_registry()->GetDefinition(func.name(), child_args);
            PL_RETURN_IF_ERROR(s);
            return s.ValueOrDie()->exec_return_type();
          })
          .Walk(*this);

  // TODO(zasgar): Why is this necessary? For some reason the proper constructor is
  // not getting invoked.
  PL_RETURN_IF_ERROR(res);
  return res.ValueOrDie();
}

std::string ScalarFunc::DebugString() const {
  std::string debug_string;
  std::vector<std::string> arg_strings;
  for (const auto &arg : arg_deps_) {
    arg_strings.push_back(arg->DebugString());
  }
  debug_string += absl::StrFormat("fn:%s(%s)", name_, absl::StrJoin(arg_strings, ","));
  return debug_string;
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
