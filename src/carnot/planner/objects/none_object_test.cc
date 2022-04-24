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

#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
class NoneObjectTest : public QLObjectTest {};
TEST_F(NoneObjectTest, TestNoMethodsWork) {
  std::shared_ptr<NoneObject> none = std::make_shared<NoneObject>(ast, ast_visitor.get());
  EXPECT_THAT(none->GetMethod("agg").status(),
              HasCompilerError("'None' object has no attribute 'agg'"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
