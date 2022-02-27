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

#include "src/carnot/planner/objects/exporter.h"

#include <functional>
#include <utility>
#include <vector>

#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<Exporter>> Exporter::Create(ASTVisitor* ast_visitor, ExportFn export_fn) {
  return std::shared_ptr<Exporter>(new Exporter(ast_visitor, export_fn));
}
Status Exporter::Export(const pypa::AstPtr& ast, Dataframe* df) { return export_fn_(ast, df); }

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
