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

#include "src/stirling/obj_tools/dwarf_reader.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace obj_tools {

using px::operator<<;

TEST(FunctionArgTracker, GolangStack) {
  FunctionArgTracker arg_tracker(ABI::kGolangStack);
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 8), (VarLocation{LocationType::kStack, 0}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(4, 4), (VarLocation{LocationType::kStack, 8}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(4, 4), (VarLocation{LocationType::kStack, 12}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(4, 4), (VarLocation{LocationType::kStack, 16}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 4), (VarLocation{LocationType::kStack, 20}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 8), (VarLocation{LocationType::kStack, 32}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(2, 2), (VarLocation{LocationType::kStack, 40}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(120, 8), (VarLocation{LocationType::kStack, 48}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 8), (VarLocation{LocationType::kStack, 168}));
}

TEST(FunctionArgTracker, SystemVAMD64) {
  FunctionArgTracker arg_tracker(ABI::kSystemVAMD64);
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 8),
                   (VarLocation{LocationType::kRegister, 0, {RegisterName::kRDI}}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(4, 4),
                   (VarLocation{LocationType::kRegister, 8, {RegisterName::kRSI}}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(4, 4),
                   (VarLocation{LocationType::kRegister, 16, {RegisterName::kRDX}}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(4, 4),
                   (VarLocation{LocationType::kRegister, 24, {RegisterName::kRCX}}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 4),
                   (VarLocation{LocationType::kRegister, 32, {RegisterName::kR8}}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 8),
                   (VarLocation{LocationType::kRegister, 40, {RegisterName::kR9}}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(2, 2), (VarLocation{LocationType::kStack, 0}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(120, 8), (VarLocation{LocationType::kStack, 8}));
  EXPECT_OK_AND_EQ(arg_tracker.PopLocation(8, 8), (VarLocation{LocationType::kStack, 128}));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
