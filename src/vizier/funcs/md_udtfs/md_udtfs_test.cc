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

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/udf/udf.h"
#include "src/carnot/udf/udf_wrapper.h"
#include "src/common/event/real_time_system.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"
#include "src/shared/upid/upid.h"
#include "src/vizier/funcs/md_udtfs/md_udtfs_impl.h"

namespace px {
namespace vizier {
namespace funcs {
namespace md {

TEST(GetProcessParentsTest, ListsParentsOfProcesses) {
  event::RealTimeSystem time_system;
  auto state = std::make_shared<px::md::AgentMetadataState>("myhost", 1, 123, sole::uuid4(),
                                                            "mypod", sole::uuid4(), "myvizier",
                                                            "myviziernamespace", &time_system);
  px::md::UPID child(1, 300, 3000);
  px::md::UPID parent(1, 100, 1000);
  state->AddProcessParent(child, parent);
  carnot::udf::FunctionContext ctx(state, nullptr);

  carnot::udf::UDTFWrapper<GetProcessParents> wrapper;
  auto udtf = wrapper.Make();
  arrow::UInt128Builder upids;
  arrow::UInt128Builder parent_upids;
  std::vector<arrow::ArrayBuilder*> outs{&upids, &parent_upids};
  EXPECT_FALSE(wrapper.ExecBatchUpdate(udtf.get(), &ctx, 100, &outs));

  std::shared_ptr<arrow::Array> upid_col;
  std::shared_ptr<arrow::Array> parent_upid_col;
  ASSERT_TRUE(upids.Finish(&upid_col).ok());
  ASSERT_TRUE(parent_upids.Finish(&parent_upid_col).ok());
  ASSERT_EQ(upid_col->length(), 1);
  EXPECT_EQ(static_cast<arrow::UInt128Array*>(upid_col.get())->Value(0), child.value());
  EXPECT_EQ(static_cast<arrow::UInt128Array*>(parent_upid_col.get())->Value(0), parent.value());
}

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace px
