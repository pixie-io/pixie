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

#include "src/carnot/plan/scalar_expression.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>

#include "src/carnot/plan/plan_state.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf_definition.h"
#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace plan {

px::Status ScalarValue::Init(const px::carnot::planpb::ScalarValue& pb) {
  DCHECK(!is_initialized_) << "Already initialized";
  CHECK(pb.data_type() != types::DATA_TYPE_UNKNOWN);
  CHECK(types::DataType_IsValid(pb.data_type()));
  // TODO(zasgar): We should probably add a check to make sure that when a given
  // DataType is set, the wrong field value is not set.

  // Copy the proto.
  pb_ = pb;
  is_initialized_ = true;
  return Status::OK();
}

// PX_CARNOT_UPDATE_FOR_NEW_TYPES

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

int64_t ScalarValue::Time64NSValue() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != planpb::ScalarValue::kTime64NsValue)
      << "Calling accessor on null/invalid value";
  return pb_.time64_ns_value();
}

absl::uint128 ScalarValue::UInt128Value() const {
  DCHECK(is_initialized_) << "Not initialized";
  VLOG_IF(1, pb_.value_case() != planpb::ScalarValue::kUint128Value)
      << "Calling accessor on null/invalid value";
  return types::UInt128Value(pb_.uint128_value()).val;
}

bool ScalarValue::IsNull() const {
  DCHECK(is_initialized_) << "Not initialized";
  return pb_.value_case() == planpb::ScalarValue::VALUE_NOT_SET;
}

// PX_CARNOT_UPDATE_FOR_NEW_TYPES
std::string ScalarValue::DebugString() const {
  DCHECK(is_initialized_) << "Not initialized";
  if (IsNull()) {
    return "<null>";
  }
  switch (DataType()) {
    case types::BOOLEAN:
      return BoolValue() ? "true" : "false";
    case types::INT64:
      return absl::Substitute("$0", Int64Value());
    case types::FLOAT64:
      return absl::Substitute("$0f", Float64Value());
    case types::STRING:
      return absl::Substitute("\"$0\"", StringValue());
    case types::TIME64NS:
      return absl::Substitute("$0", Time64NSValue());
    case types::UINT128:
      return absl::Substitute("hi:$0,lo:$1", absl::Uint128High64(UInt128Value()),
                              absl::Uint128Low64(UInt128Value()));
    default:
      return "<Unknown>";
  }
}

std::shared_ptr<types::BaseValueType> ScalarValue::ToBaseValueType() const {
  switch (DataType()) {
    case types::BOOLEAN:
      return std::make_shared<types::BoolValue>(BoolValue());
    case types::INT64:
      return std::make_shared<types::Int64Value>(Int64Value());
    case types::FLOAT64:
      return std::make_shared<types::Float64Value>(Float64Value());
    case types::STRING:
      return std::make_shared<types::StringValue>(StringValue());
    case types::TIME64NS:
      return std::make_shared<types::Time64NSValue>(Time64NSValue());
    case types::UINT128:
      return std::make_shared<types::UInt128Value>(UInt128Value());
    default:
      CHECK(0) << "Unknown data type";
  }
}

StatusOr<types::DataType> ScalarValue::OutputDataType(const PlanState&,
                                                      const table_store::schema::Schema&) const {
  DCHECK(is_initialized_) << "Not initialized";
  return DataType();
}

std::vector<const Column*> ScalarValue::ColumnDeps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

std::vector<ScalarExpression*> ScalarValue::Deps() const {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

Expression ScalarValue::ExpressionType() const { return Expression::kConstant; }

Status Column::Init(const planpb::Column& pb) {
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
  return absl::Substitute("node<$0>::col[$1]", NodeID(), Index());
}

StatusOr<types::DataType> Column::OutputDataType(
    const PlanState&, const table_store::schema::Schema& input_schema) const {
  DCHECK(is_initialized_) << "Not initialized";
  StatusOr<const table_store::schema::Relation> s = input_schema.GetRelation(NodeID());

  PX_RETURN_IF_ERROR(s);
  const auto& relation = s.ValueOrDie();
  if (!relation.HasColumn(Index())) {
    return error::InvalidArgument("Mismatch between plan and table schema");
  }
  types::DataType dt = relation.GetColumnType(Index());
  return dt;
}

std::vector<const Column*> Column::ColumnDeps() {
  DCHECK(is_initialized_) << "Not initialized";
  return {this};
}

Expression Column::ExpressionType() const { return Expression::kColumn; }

std::vector<ScalarExpression*> Column::Deps() const {
  DCHECK(is_initialized_) << "Not initialized";
  return {};
}

template <typename T, typename TProto>
StatusOr<std::unique_ptr<ScalarExpression>> MakeExprHelper(const TProto& pb) {
  auto expr = std::make_unique<T>();
  auto s = expr->Init(pb);
  PX_RETURN_IF_ERROR(s);
  return std::unique_ptr<ScalarExpression>(std::move(expr));
}

StatusOr<std::unique_ptr<ScalarExpression>> ScalarExpression::FromProto(
    const planpb::ScalarExpression& pb) {
  switch (pb.value_case()) {
    case planpb::ScalarExpression::kColumn:
      return MakeExprHelper<Column>(pb.column());
    case planpb::ScalarExpression::kConstant:
      return MakeExprHelper<ScalarValue>(pb.constant());
    case planpb::ScalarExpression::kFunc:
      return MakeExprHelper<ScalarFunc>(pb.func());
    default:
      return error::Unimplemented("Expression type: %d", pb.value_case());
  }
}

Status ScalarFunc::Init(const planpb::ScalarFunc& pb) {
  DCHECK_EQ(pb.args_size(), pb.args_data_types_size());
  name_ = pb.name();
  udf_id_ = pb.id();
  for (const auto& arg : pb.args()) {
    auto s = ScalarExpression::FromProto(arg);
    if (!s.ok()) {
      return s.status();
    }
    arg_deps_.emplace_back(s.ConsumeValueOrDie());
  }
  for (int i = 0; i < pb.init_args_size(); ++i) {
    ScalarValue sv;
    PX_RETURN_IF_ERROR(sv.Init(pb.init_args(i)));
    init_arguments_.push_back(sv);

    registry_arg_types_.push_back(pb.init_args(i).data_type());
  }
  for (int64_t i = 0; i < pb.args_data_types_size(); i++) {
    registry_arg_types_.push_back(pb.args_data_types(i));
  }
  return Status::OK();
}

std::vector<ScalarExpression*> ScalarFunc::Deps() const {
  std::vector<ScalarExpression*> deps;
  for (const auto& arg : arg_deps_) {
    // No ownership transfer.
    deps.emplace_back(arg.get());
  }
  return deps;
}

Expression ScalarFunc::ExpressionType() const { return Expression::kFunc; }

std::vector<const Column*> ScalarFunc::ColumnDeps() {
  std::vector<const Column*> cols;
  ExpressionWalker<int>()
      .OnColumn([&](const auto& col, const auto&) {
        cols.push_back(&col);
        return 0;
      })
      .Walk(*this);
  return cols;
}

StatusOr<types::DataType> ScalarFunc::OutputDataType(
    const PlanState& state, const table_store::schema::Schema& input_schema) const {
  // The output data type of a function is based on the computed types of the children
  // followed by the looking up the function in the registry and getting the output
  // data type of the function.
  auto res = ExpressionWalker<StatusOr<types::DataType>>()
                 .OnScalarValue([&](auto& val, auto&) -> StatusOr<types::DataType> {
                   return val.OutputDataType(state, input_schema);
                 })
                 .OnColumn([&](auto& col, auto&) -> StatusOr<types::DataType> {
                   return col.OutputDataType(state, input_schema);
                 })
                 .OnScalarFunc([&](auto& func, auto& child_results) -> StatusOr<types::DataType> {
                   std::vector<types::DataType> registry_args;
                   registry_args.reserve(func.init_arguments().size() + child_results.size());
                   for (const auto& init_arg : func.init_arguments()) {
                     registry_args.push_back(init_arg.DataType());
                   }
                   for (const auto& child_result : child_results) {
                     PX_RETURN_IF_ERROR(child_result);
                     registry_args.push_back(child_result.ValueOrDie());
                   }
                   auto s =
                       state.func_registry()->GetScalarUDFDefinition(func.name(), registry_args);
                   PX_RETURN_IF_ERROR(s);
                   return s.ValueOrDie()->exec_return_type();
                 })
                 .Walk(*this);

  // TODO(zasgar): Why is this necessary? For some reason the proper constructor is
  // not getting invoked.
  PX_RETURN_IF_ERROR(res);
  return res.ValueOrDie();
}

std::string ScalarFunc::DebugString() const {
  std::string debug_string;
  std::vector<std::string> arg_strings;
  for (const auto& arg : arg_deps_) {
    arg_strings.push_back(arg->DebugString());
  }
  debug_string += absl::Substitute("fn:$0($1)", name_, absl::StrJoin(arg_strings, ","));
  return debug_string;
}

Status AggregateExpression::Init(const planpb::AggregateExpression& pb) {
  name_ = pb.name();
  uda_id_ = pb.id();
  for (const auto& arg : pb.args()) {
    // arg is of message type AggregateExpression.Arg. Needs to be casted to a ScalarExpression.
    planpb::ScalarExpression se;

    se.ParseFromString(arg.SerializeAsString());
    auto s = ScalarExpression::FromProto(se);
    if (!s.ok()) {
      return s.status();
    }
    arg_deps_.emplace_back(s.ConsumeValueOrDie());
  }
  for (int64_t i = 0; i < pb.init_args_size(); ++i) {
    ScalarValue sv;
    PX_RETURN_IF_ERROR(sv.Init(pb.init_args(i)));
    init_arguments_.push_back(sv);

    registry_arg_types_.push_back(pb.init_args(i).data_type());
  }
  for (int64_t i = 0; i < pb.args_data_types_size(); i++) {
    registry_arg_types_.push_back(pb.args_data_types(i));
  }
  return Status::OK();
}

Expression AggregateExpression::ExpressionType() const { return Expression::kAgg; }

std::vector<ScalarExpression*> AggregateExpression::Deps() const {
  std::vector<ScalarExpression*> deps;
  for (const auto& arg : arg_deps_) {
    // No ownership transfer.
    deps.emplace_back(arg.get());
  }
  return deps;
}

std::vector<const Column*> AggregateExpression::ColumnDeps() {
  std::vector<const Column*> cols;
  for (const auto& arg : arg_deps_) {
    auto dep = arg.get();
    if (dep->ExpressionType() == Expression::kColumn) {
      const auto* col = static_cast<const Column*>(dep);
      cols.push_back(col);
    }
  }
  return cols;
}

StatusOr<types::DataType> AggregateExpression::OutputDataType(
    const PlanState& state, const table_store::schema::Schema&) const {
  // The output data type of a function is based on the computed types of the init_args + args
  // followed by the looking up the function in the registry and getting the output
  // data type of the function.
  PX_ASSIGN_OR_RETURN(auto s, state.func_registry()->GetUDADefinition(name_, registry_arg_types_));
  return s->finalize_return_type();
}

std::string AggregateExpression::DebugString() const {
  std::string debug_string;
  std::vector<std::string> arg_strings;
  for (const auto& arg : arg_deps_) {
    arg_strings.push_back(arg->DebugString());
  }
  debug_string +=
      absl::Substitute("aggregate expression:$0($1)", name_, absl::StrJoin(arg_strings, ","));
  return debug_string;
}

}  // namespace plan
}  // namespace carnot
}  // namespace px
