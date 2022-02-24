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

using ::px::operator<<;

TEST(GolangStackABIModel, FunctionParameters) {
  std::unique_ptr<ABICallingConventionModel> abi_model =
      ABICallingConventionModel::Create(ABI::kGolangStack);
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kStack, 0}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kStack, 8}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kStack, 12}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kStack, 16}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 4, 2, false),
                   (VarLocation{LocationType::kStack, 20}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kStack, 32}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 2, 2, 1, false),
                   (VarLocation{LocationType::kStack, 40}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 120, 8, 15, false),
                   (VarLocation{LocationType::kStack, 48}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kStack, 168}));
}

TEST(GolangRegisterABIModel, FunctionParameters) {
  std::unique_ptr<ABICallingConventionModel> abi_model =
      ABICallingConventionModel::Create(ABI::kGolangRegister);
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kRegister, 0, {RegisterName::kRAX}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kRegister, 8, {RegisterName::kRBX}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kRegister, 16, {RegisterName::kRCX}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kRegister, 24, {RegisterName::kRDI}}));
  EXPECT_OK_AND_EQ(
      abi_model->PopLocation(TypeClass::kInteger, 8, 4, 2, false),
      (VarLocation{LocationType::kRegister, 32, {RegisterName::kRSI, RegisterName::kR8}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kRegister, 48, {RegisterName::kR9}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 2, 2, 1, false),
                   (VarLocation{LocationType::kRegister, 56, {RegisterName::kR10}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 120, 8, 15, false),
                   (VarLocation{LocationType::kStack, 0}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kRegister, 64, {RegisterName::kR11}}));
}

TEST(SystemVAMD64ABIModel, FunctionParameters) {
  std::unique_ptr<ABICallingConventionModel> abi_model =
      ABICallingConventionModel::Create(ABI::kSystemVAMD64);
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kRegister, 0, {RegisterName::kRDI}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kRegister, 8, {RegisterName::kRSI}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kRegister, 16, {RegisterName::kRDX}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 4, 4, 1, false),
                   (VarLocation{LocationType::kRegister, 24, {RegisterName::kRCX}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 4, 2, false),
                   (VarLocation{LocationType::kRegister, 32, {RegisterName::kR8}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kRegister, 40, {RegisterName::kR9}}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 2, 2, 1, false),
                   (VarLocation{LocationType::kStack, 0}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 120, 8, 15, false),
                   (VarLocation{LocationType::kStack, 8}));
  EXPECT_OK_AND_EQ(abi_model->PopLocation(TypeClass::kInteger, 8, 8, 1, false),
                   (VarLocation{LocationType::kStack, 128}));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
