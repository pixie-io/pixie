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

#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/module.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using ModuleTest = QLObjectTest;

constexpr char kModulePxl[] = R"pxl(
import px
def funcs():
    '''
    Merge the func helpers together
    '''
    udf = px.GetUDFList()
    uda = px.GetUDAList()
    udtf = px.GetUDTFList()
    udf.func_type = "UDF"
    uda.func_type = "UDA"
    udtf.func_type = "UDTF"
    udf_uda = udf.append(uda)
    udf_uda.output_relation = udf_uda.return_type
    udf_uda.executor = 'N/A'
    udf_uda.init_args = udf_uda.args
    return udf_uda.drop(['return_type', 'args']).append(udtf)

free_var = "imfree"
)pxl";

TEST_F(ModuleTest, load_and_use_modules) {
  ASSERT_OK_AND_ASSIGN(auto module_obj, Module::Create(kModulePxl, ast_visitor.get()));
  var_table->Add("testmodule", module_obj);
  ASSERT_OK_AND_ASSIGN(auto funcresult, ParseExpression("testmodule.funcs()"));
  auto union_obj = static_cast<Dataframe*>(funcresult.get());

  EXPECT_MATCH(union_obj->op(), Union());

  // Make sure "free_var" is accessible as an attribute.
  ASSERT_OK_AND_ASSIGN(auto free_var, ParseExpression("testmodule.free_var"));
  ASSERT_TRUE(ExprObject::IsExprObject(free_var));
  EXPECT_MATCH(static_cast<ExprObject*>(free_var.get())->expr(), String("imfree"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
