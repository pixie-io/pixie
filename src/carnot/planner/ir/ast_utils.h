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
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/any.h>
#include <pypa/ast/ast.hh>
#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {

#define PYPA_PTR_CAST(TYPE, VAL) \
  std::static_pointer_cast<typename pypa::AstTypeByID<pypa::AstType::TYPE>::Type>(VAL)

#define PYPA_CAST(TYPE, VAL) static_cast<AstTypeByID<AstType::TYPE>::Type&>(VAL)

/**
 * @brief Create an error that incorporates line, column of ast node into the error message.
 * @param ast the ast value to use to create an error.
 * @param err_msg the error message to store.
 * @return Status the formatted error
 */
template <typename... Args>
Status CreateAstError(const pypa::AstPtr& ast, Args... args) {
  auto msg = absl::Substitute(args...);
  compilerpb::CompilerErrorGroup context = LineColErrorPb(ast->line, ast->column, msg);
  return Status(statuspb::INVALID_ARGUMENT, msg,
                std::make_unique<compilerpb::CompilerErrorGroup>(context));
}

template <typename... Args>
Status CreateAstError(const pypa::Ast& ast, Args... args) {
  return CreateAstError(std::make_shared<pypa::Ast>(ast), args...);
}

/**
 * @brief Get the string repr of the passed in type.
 *
 * @param type
 * @return std::string
 */
std::string GetAstTypeName(pypa::AstType type);

/**
 * @brief Get the Id from the NameAST.
 *
 * @param node
 * @return std::string
 */
std::string GetNameAsString(const pypa::AstPtr& node);

/**
 * @brief Gets the string out of what is suspected to be a strAst. Errors out if ast is not of
 * type str.
 *
 * @param ast
 * @return StatusOr<std::string>
 */
StatusOr<std::string> GetStrAstValue(const pypa::AstPtr& ast);

/**
 * @brief Wraps a status with a line, col error with the line,col in the ast. If there is a context
 * in the status, it doesn't wrap it because we assume there's already a line,col error for the
 * status.
 *
 * @param ast
 * @param status
 * @return Status
 */
Status WrapAstError(const pypa::AstPtr& ast, Status status);

/**
 * @brief Wraps an error with "outer context" which means we can get a stack trace of where an error
 * might come from.
 * @param ast the ast pointer to wrap with.
 * @param str the new error to add.
 * @param status the status to wrap.
 * @return Status the original status wrapped with the new one.
 */
template <typename... Args>
Status AddOuterContextToError(Status status, const pypa::AstPtr& ast, Args... args) {
  if (status.ok()) {
    return status;
  }
  if (!status.has_context()) {
    status = WrapAstError(ast, status);
  }
  if (!status.context()->Is<compilerpb::CompilerErrorGroup>()) {
    return status;
  }
  compilerpb::CompilerErrorGroup error_group;
  CHECK(status.context()->UnpackTo(&error_group));
  auto msg = absl::Substitute(args...);
  AddLineColError(&error_group, ast->line, ast->column, msg);

  return Status(statuspb::INVALID_ARGUMENT, msg,
                std::make_unique<compilerpb::CompilerErrorGroup>(error_group));
}

template <typename T>
StatusOr<T> WrapError(const pypa::AstPtr& ast, StatusOr<T> status_or) {
  if (status_or.ok() || status_or.status().has_context()) {
    return status_or;
  }
  return CreateAstError(ast, status_or.msg());
}

StatusOr<std::string> AstToString(const std::shared_ptr<pypa::Ast>& ast);

}  // namespace planner
}  // namespace carnot
}  // namespace px
