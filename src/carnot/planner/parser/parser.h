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
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * Parser converts a query into an AST or errors out.
 */
class Parser {
 public:
  /**
   * Parses the query into an ast.
   * @param query the query to compile.
   * @return the ast module that represents the query.
   */
  StatusOr<pypa::AstModulePtr> Parse(std::string_view query, bool parse_doc_strings = true);
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
