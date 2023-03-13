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

#include <sole.hpp>

#include "src/carnot/planner/ir/bool_ir.h"
#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/float_ir.h"
#include "src/carnot/planner/ir/int_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/string_ir.h"
#include "src/carnot/planner/ir/time_ir.h"
#include "src/carnot/planner/ir/uint128_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status DataIR::ToProto(planpb::ScalarExpression* expr) const {
  auto value_pb = expr->mutable_constant();
  return ToProto(value_pb);
}

Status DataIR::ToProto(planpb::ScalarValue* value_pb) const {
  value_pb->set_data_type(evaluated_data_type_);
  return ToProtoImpl(value_pb);
}

uint64_t DataIR::HashValue() const { return HashValueImpl(); }

StatusOr<DataIR*> DataIR::FromProto(IR* ir, std::string_view name,
                                    const planpb::ScalarValue& value) {
  switch (value.data_type()) {
    case types::BOOLEAN: {
      return ir->CreateNode<BoolIR>(/*ast*/ nullptr, value.bool_value());
    }
    case types::INT64: {
      return ir->CreateNode<IntIR>(/*ast*/ nullptr, value.int64_value());
    }
    case types::FLOAT64: {
      return ir->CreateNode<FloatIR>(/*ast*/ nullptr, value.float64_value());
    }
    case types::STRING: {
      return ir->CreateNode<StringIR>(/*ast*/ nullptr, value.string_value());
    }
    case types::UINT128: {
      std::string upid_str =
          sole::rebuild(value.uint128_value().high(), value.uint128_value().low()).str();
      return ir->CreateNode<UInt128IR>(/*ast*/ nullptr, upid_str);
    }
    case types::TIME64NS: {
      return ir->CreateNode<TimeIR>(/*ast*/ nullptr, value.time64_ns_value());
    }
    default: {
      return error::InvalidArgument("Error processing $0: $1 not handled as a default data type.",
                                    name, types::ToString(value.data_type()));
    }
  }
}

StatusOr<DataIR*> DataIR::ZeroValueForType(IR* ir, types::DataType type) {
  PX_ASSIGN_OR_RETURN(auto ir_type, DataTypeToIRNodeType(type));
  return ZeroValueForType(ir, ir_type);
}

StatusOr<DataIR*> DataIR::ZeroValueForType(IR* ir, IRNodeType type) {
  switch (type) {
    case IRNodeType::kBool:
      return ir->CreateNode<BoolIR>(/*ast*/ nullptr, false);
    case IRNodeType::kFloat:
      return ir->CreateNode<FloatIR>(/*ast*/ nullptr, 0);
    case IRNodeType::kInt:
      return ir->CreateNode<IntIR>(/*ast*/ nullptr, 0);
    case IRNodeType::kString:
      return ir->CreateNode<StringIR>(/*ast*/ nullptr, "");
    case IRNodeType::kTime:
      return ir->CreateNode<TimeIR>(/*ast*/ nullptr, 0);
    case IRNodeType::kUInt128:
      return ir->CreateNode<UInt128IR>(/*ast*/ nullptr, 0);
    default:
      CHECK(false) << absl::Substitute("Invalid IRNodeType for DataIR: $0", TypeString(type));
  }
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
