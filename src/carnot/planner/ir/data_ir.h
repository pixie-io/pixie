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

#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class DataIR : public ExpressionIR {
 public:
  static types::DataType DataType(IRNodeType type) {
    return IRNodeTypeToDataType(type).ConsumeValueOrDie();
  }

  types::DataType EvaluatedDataType() const override { return evaluated_data_type_; }
  bool IsDataTypeEvaluated() const override { return true; }
  bool IsData() const override { return true; }

  /**
   * @brief ToProto method override that writes ScalarValues to the ScalarExpression message.
   *
   * @param expr
   * @return Status
   */
  Status ToProto(planpb::ScalarExpression* expr) const override;

  /**
   * @brief ToProto method that takes in a scalar message instead of a ScalarExpression.
   *
   * @param column_pb
   * @return Status
   */
  Status ToProto(planpb::ScalarValue* column_pb) const;

  static StatusOr<DataIR*> FromProto(IR* ir, std::string_view name,
                                     const planpb::ScalarValue& value);

  /**
   * @brief HashValue returns a 64bit hash of the value of this Data node.
   * @return uint64_t hash value
   */
  uint64_t HashValue() const;

  /**
   * @brief The implementation of ToProto for DataIR derived classes.
   * Each implementation should only be one line such as
   * `value->set_int64_value`
   *
   * @param value the value protobuf to use.
   * @return Status
   */
  virtual Status ToProtoImpl(planpb::ScalarValue* value) const = 0;

  static StatusOr<DataIR*> ZeroValueForType(IR* ir, IRNodeType type);
  static StatusOr<DataIR*> ZeroValueForType(IR* ir, types::DataType type);

 protected:
  DataIR(int64_t id, IRNodeType type, const ExpressionIR::Annotations& annotations)
      : ExpressionIR(id, type, annotations), evaluated_data_type_(DataType(type)) {}
  virtual uint64_t HashValueImpl() const = 0;

 private:
  types::DataType evaluated_data_type_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
