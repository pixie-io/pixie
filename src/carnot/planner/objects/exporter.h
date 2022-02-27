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

#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

// ExportFn takes in a DataFrame then exports it somehow.
using ExportFn = std::function<Status(const pypa::AstPtr& ast, Dataframe*)>;
/**
 * @brief Exporter is an object that describes where
 * to export a DataFrame. The exporter object is initialized
 * with a function that is called on the dataframe.
 * The function is responsible for exporting the data
 * but there are no formal checks that the data is actually
 * exported.
 */
class Exporter : public QLObject {
 public:
  static constexpr TypeDescriptor ExporterType = {
      /* name */ "Exporter",
      /* type */ QLObjectType::kExporter,
  };
  static StatusOr<std::shared_ptr<Exporter>> Create(ASTVisitor* ast_visitor, ExportFn export_fn);
  static bool IsExporter(const QLObjectPtr& object) {
    return object->type() == ExporterType.type();
  }

  // Constant for the modules.
  inline static constexpr char kExporterOpID[] = "Exporter";

  Status Export(const pypa::AstPtr& ast, Dataframe* df);

 protected:
  explicit Exporter(ASTVisitor* ast_visitor, ExportFn export_fn)
      : QLObject(ExporterType, ast_visitor), export_fn_(std::move(export_fn)) {}

 private:
  ExportFn export_fn_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
