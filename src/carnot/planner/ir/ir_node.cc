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

#include "src/carnot/planner/ir/ir_node.h"

namespace px {
namespace carnot {
namespace planner {

Status IRNode::CopyFromNode(const IRNode* node,
                            absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  line_ = node->line_;
  col_ = node->col_;
  line_col_set_ = node->line_col_set_;
  ast_ = node->ast_;
  resolved_type_ = node->resolved_type_;
  return CopyFromNodeImpl(node, copied_nodes_map);
}

void IRNode::SetLineCol(int64_t line, int64_t col) {
  line_ = line;
  col_ = col;
  line_col_set_ = true;
}
void IRNode::SetLineCol(const pypa::AstPtr& ast) {
  ast_ = ast;
  SetLineCol(ast->line, ast->column);
}

std::string IRNode::DebugString() const {
  return absl::Substitute("$0(id=$1)", type_string(), id());
}

StatusOr<px::types::DataType> IRNodeTypeToDataType(IRNodeType type) {
  switch (type) {
    case IRNodeType::kString: {
      return px::types::DataType::STRING;
    }
    case IRNodeType::kInt: {
      return px::types::DataType::INT64;
    }
    case IRNodeType::kUInt128: {
      return px::types::DataType::UINT128;
    }
    case IRNodeType::kBool: {
      return px::types::DataType::BOOLEAN;
    }
    case IRNodeType::kFloat: {
      return px::types::DataType::FLOAT64;
    }
    case IRNodeType::kTime: {
      return px::types::DataType::TIME64NS;
    }
    default: {
      return error::InvalidArgument("IRNode type: $0 cannot be converted into literal type.",
                                    magic_enum::enum_name(type));
    }
  }
}

StatusOr<IRNodeType> DataTypeToIRNodeType(types::DataType type) {
  switch (type) {
    case px::types::DataType::STRING: {
      return IRNodeType::kString;
    }
    case px::types::DataType::INT64: {
      return IRNodeType::kInt;
    }
    case px::types::DataType::UINT128: {
      return IRNodeType::kUInt128;
    }
    case px::types::DataType::BOOLEAN: {
      return IRNodeType::kBool;
    }
    case px::types::DataType::FLOAT64: {
      return IRNodeType::kFloat;
    }
    case px::types::DataType::TIME64NS: {
      return IRNodeType::kTime;
    }
    default: {
      return error::InvalidArgument("data type: '$0' cannot be converted into an IRNodeType",
                                    types::ToString(type));
    }
  }
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
