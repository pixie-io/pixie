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
#include <utility>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/exporter.h"
#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/var_table.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class ExporterTest : public QLObjectTest {};

TEST_F(ExporterTest, create_and_export_span) {
  std::string script = R"pxl(
import px
df = px.DataFrame('http_events')
px.export(df, exporter)
)pxl";
  // We check to make sure the exporter lambda actually runs by checking the table_name value after
  // the script is parsed.
  std::string table_name;
  ASSERT_OK_AND_ASSIGN(
      auto exporter,
      Exporter::Create(ast_visitor.get(), [&table_name](auto&&, auto&& df) -> Status {
        auto op = std::forward<decltype(df)>(df)->op();
        CHECK(Match(op, MemorySource()));
        table_name = static_cast<MemorySourceIR*>(op)->table_name();
        return Status::OK();
      }));
  auto var_table = VarTable::Create();
  var_table->Add("exporter", exporter);
  ASSERT_OK(ParseScript(var_table, script));
  EXPECT_EQ(table_name, "http_events");
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
