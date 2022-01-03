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

constexpr std::string_view kTestGo1_16Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_16_binary";
constexpr std::string_view kTestGo1_17Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_17_binary";
constexpr std::string_view kGoGRPCServer =
    "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/golang_1_16_grpc_tls_server_binary/go/"
    "src/grpc_tls_server/grpc_tls_server";
constexpr std::string_view kCppBinary = "src/stirling/obj_tools/testdata/cc/test_exe";
constexpr std::string_view kGoBinaryUnconventional =
    "src/stirling/obj_tools/testdata/go/sockshop_payments_service";

namespace px {
namespace stirling {
namespace obj_tools {

using ::llvm::DWARFDie;
using ::px::stirling::obj_tools::DwarfReader;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// Automatically converts ToString() to stream operator for gtest.
using ::px::operator<<;

struct DwarfReaderTestParam {
  bool index;
};

auto CreateDwarfReader(const std::filesystem::path& path, bool indexing) {
  if (indexing) {
    return DwarfReader::CreateIndexingAll(path);
  }
  return DwarfReader::CreateWithoutIndexing(path);
}

class DwarfReaderTest : public ::testing::TestWithParam<DwarfReaderTestParam> {
 protected:
  DwarfReaderTest()
      : kCppBinaryPath(px::testing::BazelBinTestFilePath(kCppBinary)),
        kGo1_16BinaryPath(px::testing::TestFilePath(kTestGo1_16Binary)),
        kGo1_17BinaryPath(px::testing::TestFilePath(kTestGo1_17Binary)),
        kGoServerBinaryPath(px::testing::BazelBinTestFilePath(kGoGRPCServer)),
        kGoBinaryUnconventionalPath(px::testing::TestFilePath(kGoBinaryUnconventional)) {}

  const std::string kCppBinaryPath;
  const std::string kGo1_16BinaryPath;
  const std::string kGo1_17BinaryPath;
  const std::string kGoServerBinaryPath;
  const std::string kGoBinaryUnconventionalPath;
};

TEST_F(DwarfReaderTest, NonExistentPath) {
  auto s = DwarfReader::CreateWithoutIndexing("/bogus");
  ASSERT_NOT_OK(s);
}

TEST_F(DwarfReaderTest, SourceLanguage) {
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::CreateWithoutIndexing(kCppBinaryPath));
    // We use C++17, but the dwarf shows 14.
    EXPECT_EQ(dwarf_reader->source_language(), llvm::dwarf::DW_LANG_C_plus_plus_14);
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("clang"));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::CreateWithoutIndexing(kGo1_16BinaryPath));
    EXPECT_EQ(dwarf_reader->source_language(), llvm::dwarf::DW_LANG_Go);
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("go"));
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::Not(::testing::HasSubstr("regabi")));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::CreateWithoutIndexing(kGo1_17BinaryPath));
    EXPECT_EQ(dwarf_reader->source_language(), llvm::dwarf::DW_LANG_Go);
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("go"));
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("regabi"));
  }
}

// Tests that GetMatchingDIEs() returns empty vector when nothing is found.
TEST_P(DwarfReaderTest, GetMatchingDIEsReturnsEmptyVector) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));
  ASSERT_OK_AND_THAT(
      dwarf_reader->GetMatchingDIEs("non-existent-name", llvm::dwarf::DW_TAG_structure_type),
      IsEmpty());
}

TEST_P(DwarfReaderTest, CppGetStructByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("ABCStruct32"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("ABCStruct64"), 24);
}

TEST_P(DwarfReaderTest, Go1_16GetStructByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_16BinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("main.Vertex"), 16);
}

TEST_P(DwarfReaderTest, Go1_17GetStructByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_17BinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("main.Vertex"), 16);
}

TEST_P(DwarfReaderTest, CppGetStructMemberInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructMemberInfo("ABCStruct32", llvm::dwarf::DW_TAG_structure_type, "b",
                                        llvm::dwarf::DW_TAG_member),
      (StructMemberInfo{4, TypeInfo{VarType::kBaseType, "int"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("ABCStruct32", llvm::dwarf::DW_TAG_structure_type,
                                                  "bogus", llvm::dwarf::DW_TAG_member));
}

TEST_P(DwarfReaderTest, Go1_16GetStructMemberInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_16BinaryPath, p.index));

  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructMemberInfo("main.Vertex", llvm::dwarf::DW_TAG_structure_type, "Y",
                                        llvm::dwarf::DW_TAG_member),
      (StructMemberInfo{8, TypeInfo{VarType::kBaseType, "float64"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("main.Vertex", llvm::dwarf::DW_TAG_structure_type,
                                                  "bogus", llvm::dwarf::DW_TAG_member));
}

TEST_P(DwarfReaderTest, Go1_17GetStructMemberInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_17BinaryPath, p.index));

  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructMemberInfo("main.Vertex", llvm::dwarf::DW_TAG_structure_type, "Y",
                                        llvm::dwarf::DW_TAG_member),
      (StructMemberInfo{8, TypeInfo{VarType::kBaseType, "float64"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("main.Vertex", llvm::dwarf::DW_TAG_structure_type,
                                                  "bogus", llvm::dwarf::DW_TAG_member));
}

TEST_P(DwarfReaderTest, CppGetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct32", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct32", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("ABCStruct32", "bogus"));
}

TEST_P(DwarfReaderTest, Go1_16GetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_16BinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("main.Vertex", "Y"), 8);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("main.Vertex", "bogus"));
}

TEST_P(DwarfReaderTest, Go1_17GetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_17BinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("main.Vertex", "Y"), 8);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("main.Vertex", "bogus"));
}

// Inspired from a real life case.
TEST_P(DwarfReaderTest, GoUnconventionalGetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGoBinaryUnconventionalPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("runtime.g", "goid"), 192);
}

TEST_P(DwarfReaderTest, CppGetStructSpec) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructSpec("OuterStruct"),
      (std::vector{
          StructSpecEntry{.offset = 0,
                          .size = 8,
                          .type_info = {.type = VarType::kBaseType, .type_name = "long int"},
                          .path = "/O0"},
          StructSpecEntry{.offset = 8,
                          .size = 1,
                          .type_info = {.type = VarType::kBaseType, .type_name = "bool"},
                          .path = "/O1/M0/L0"},
          StructSpecEntry{.offset = 12,
                          .size = 4,
                          .type_info = {.type = VarType::kBaseType, .type_name = "int"},
                          .path = "/O1/M0/L1"},
          StructSpecEntry{.offset = 16,
                          .size = 8,
                          .type_info = {.type = VarType::kPointer, .type_name = "long int*"},
                          .path = "/O1/M0/L2"},
          StructSpecEntry{.offset = 24,
                          .size = 1,
                          .type_info = {.type = VarType::kBaseType, .type_name = "bool"},
                          .path = "/O1/M1"},
          StructSpecEntry{.offset = 32,
                          .size = 1,
                          .type_info = {.type = VarType::kBaseType, .type_name = "bool"},
                          .path = "/O1/M2/L0"},
          StructSpecEntry{.offset = 36,
                          .size = 4,
                          .type_info = {.type = VarType::kBaseType, .type_name = "int"},
                          .path = "/O1/M2/L1"},
          StructSpecEntry{.offset = 40,
                          .size = 8,
                          .type_info = {.type = VarType::kPointer, .type_name = "long int*"},
                          .path = "/O1/M2/L2"},
      }));

  EXPECT_NOT_OK(dwarf_reader->GetStructSpec("Bogus"));
}

TEST_P(DwarfReaderTest, GoGetStructSpec) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_17BinaryPath, p.index));

  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructSpec("main.OuterStruct"),
      (std::vector{
          StructSpecEntry{.offset = 0,
                          .size = 8,
                          .type_info = {.type = VarType::kBaseType, .type_name = "int64"},
                          .path = "/O0"},
          StructSpecEntry{.offset = 8,
                          .size = 1,
                          .type_info = {.type = VarType::kBaseType, .type_name = "bool"},
                          .path = "/O1/M0/L0"},
          StructSpecEntry{.offset = 12,
                          .size = 4,
                          .type_info = {.type = VarType::kBaseType, .type_name = "int32"},
                          .path = "/O1/M0/L1"},
          StructSpecEntry{.offset = 16,
                          .size = 8,
                          .type_info = {.type = VarType::kPointer, .type_name = "*int64"},
                          .path = "/O1/M0/L2"},
          StructSpecEntry{.offset = 24,
                          .size = 1,
                          .type_info = {.type = VarType::kBaseType, .type_name = "bool"},
                          .path = "/O1/M1"},
          StructSpecEntry{.offset = 32,
                          .size = 1,
                          .type_info = {.type = VarType::kBaseType, .type_name = "bool"},
                          .path = "/O1/M2/L0"},
          StructSpecEntry{.offset = 36,
                          .size = 4,
                          .type_info = {.type = VarType::kBaseType, .type_name = "int32"},
                          .path = "/O1/M2/L1"},
          StructSpecEntry{.offset = 40,
                          .size = 8,
                          .type_info = {.type = VarType::kPointer, .type_name = "*int64"},
                          .path = "/O1/M2/L2"},
      }));

  EXPECT_NOT_OK(dwarf_reader->GetStructSpec("main.Bogus"));
}

TEST_P(DwarfReaderTest, CppArgumentTypeByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("CanYouFindThis", "a"), 4);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("ABCSum32", "x"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "a"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "x"), 8);
}

TEST_P(DwarfReaderTest, Golang1_16ArgumentTypeByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_16BinaryPath, p.index));

  // v is of type *Vertex.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.(*Vertex).Scale", "v"), 8);
  // f is of type float64.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.(*Vertex).Scale", "f"), 8);
  // v is of type Vertex.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.Vertex.Abs", "v"), 16);
}

TEST_P(DwarfReaderTest, Golang1_17ArgumentTypeByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_17BinaryPath, p.index));

  // v is of type *Vertex.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.(*Vertex).Scale", "v"), 8);
  // f is of type float64.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.(*Vertex).Scale", "f"), 8);
  // v is of type Vertex.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.Vertex.Abs", "v"), 16);
}

TEST_P(DwarfReaderTest, CppArgumentLocation) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("ABCSum32", "x"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 32}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("ABCSum32", "y"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 64}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("CanYouFindThis", "a"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 4}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("CanYouFindThis", "b"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("SomeFunctionWithPointerArgs", "a"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("SomeFunctionWithPointerArgs", "x"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 16}));
}

TEST_P(DwarfReaderTest, Golang1_16ArgumentLocation) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_16BinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "v"),
                   (VarLocation{.loc_type = LocationType::kStack, .offset = 0}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "f"),
                   (VarLocation{.loc_type = LocationType::kStack, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v"),
                   (VarLocation{.loc_type = LocationType::kStack, .offset = 0}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v2"),
                   (VarLocation{.loc_type = LocationType::kStack, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "f"),
                   (VarLocation{.loc_type = LocationType::kStack, .offset = 24}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.Vertex.Abs", "v"),
                   (VarLocation{.loc_type = LocationType::kStack, .offset = 0}));
}

TEST_P(DwarfReaderTest, Golang1_17ArgumentLocation) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_17BinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "v"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 0}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "f"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 17}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 0}));

  // TODO(oazizi): Support multi-piece arguments that span multiple registers.
  EXPECT_NOT_OK(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v2"));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "f"),
                   (VarLocation{.loc_type = LocationType::kRegister, .offset = 19}));

  // TODO(oazizi): Support multi-piece arguments that span multiple registers.
  EXPECT_NOT_OK(dwarf_reader->GetArgumentLocation("main.Vertex.Abs", "v"));
}

// Note the differences here and the results in CppArgumentStackPointerOffset.
// This needs more investigation. Appears as though there are issues with alignment and
// also the reference point of the offset.
TEST_P(DwarfReaderTest, CppFunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_THAT(
      dwarf_reader->GetFunctionArgInfo("CanYouFindThis"),
      UnorderedElementsAre(Pair("a", ArgInfo{TypeInfo{VarType::kBaseType, "int", "int"},
                                             {LocationType::kRegister, 0, {RegisterName::kRDI}}}),
                           Pair("b", ArgInfo{TypeInfo{VarType::kBaseType, "int", "int"},
                                             {LocationType::kRegister, 8, {RegisterName::kRSI}}})));
  EXPECT_OK_AND_THAT(
      dwarf_reader->GetFunctionArgInfo("ABCSum32"),
      UnorderedElementsAre(
          Pair("x",
               ArgInfo{TypeInfo{VarType::kStruct, "ABCStruct32", "ABCStruct32"},
                       {LocationType::kRegister, 0, {RegisterName::kRDI, RegisterName::kRSI}}}),
          Pair("y",
               ArgInfo{TypeInfo{VarType::kStruct, "ABCStruct32", "ABCStruct32"},
                       {LocationType::kRegister, 16, {RegisterName::kRDX, RegisterName::kRCX}}})));
  EXPECT_OK_AND_THAT(
      dwarf_reader->GetFunctionArgInfo("SomeFunctionWithPointerArgs"),
      UnorderedElementsAre(
          Pair("a", ArgInfo{TypeInfo{VarType::kPointer, "int*", "int*"},
                            {LocationType::kRegister, 0, {RegisterName::kRDI}}}),
          Pair("x", ArgInfo{TypeInfo{VarType::kPointer, "ABCStruct32*", "ABCStruct32*"},
                            {LocationType::kRegister, 8, {RegisterName::kRSI}}})));
}

TEST_P(DwarfReaderTest, CppFunctionRetValInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("CanYouFindThis"),
                   (RetValInfo{TypeInfo{VarType::kBaseType, "int"}, 4}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("ABCSum32"),
                   (RetValInfo{TypeInfo{VarType::kStruct, "ABCStruct32"}, 12}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("SomeFunctionWithPointerArgs"),
                   (RetValInfo{TypeInfo{VarType::kVoid, ""}, 0}));
}

TEST_P(DwarfReaderTest, Go1_16FunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         CreateDwarfReader(kGo1_16BinaryPath, p.index));

    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.(*Vertex).Scale"),
        UnorderedElementsAre(Pair("v", ArgInfo{TypeInfo{VarType::kPointer, "*main.Vertex"},
                                               {LocationType::kStack, 0}}),
                             Pair("f", ArgInfo{TypeInfo{VarType::kBaseType, "float64"},
                                               {LocationType::kStack, 8}})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.(*Vertex).CrossScale"),
        UnorderedElementsAre(Pair("v", ArgInfo{TypeInfo{VarType::kPointer, "*main.Vertex"},
                                               {LocationType::kStack, 0}}),
                             Pair("v2", ArgInfo{TypeInfo{VarType::kStruct, "main.Vertex"},
                                                {LocationType::kStack, 8}}),
                             Pair("f", ArgInfo{TypeInfo{VarType::kBaseType, "float64"},
                                               {LocationType::kStack, 24}})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.Vertex.Abs"),
        UnorderedElementsAre(Pair("v", ArgInfo{TypeInfo{VarType::kStruct, "main.Vertex"},
                                               {LocationType::kStack, 0}}),
                             Pair("~r0", ArgInfo{TypeInfo{VarType::kBaseType, "float64"},
                                                 {LocationType::kStack, 16},
                                                 true})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.MixedArgTypes"),
        UnorderedElementsAre(
            Pair("i1", ArgInfo{TypeInfo{VarType::kBaseType, "int"}, {LocationType::kStack, 0}}),
            Pair("b1", ArgInfo{TypeInfo{VarType::kBaseType, "bool"}, {LocationType::kStack, 8}}),
            Pair("b2", ArgInfo{TypeInfo{VarType::kStruct, "main.BoolWrapper"},
                               {LocationType::kStack, 9}}),
            Pair("i2", ArgInfo{TypeInfo{VarType::kBaseType, "int"}, {LocationType::kStack, 16}}),
            Pair("i3", ArgInfo{TypeInfo{VarType::kBaseType, "int"}, {LocationType::kStack, 24}}),
            Pair("b3", ArgInfo{TypeInfo{VarType::kBaseType, "bool"}, {LocationType::kStack, 32}}),
            Pair("~r6",
                 ArgInfo{TypeInfo{VarType::kBaseType, "int"}, {LocationType::kStack, 40}, true}),
            Pair("~r7", ArgInfo{TypeInfo{VarType::kStruct, "main.BoolWrapper"},
                                {LocationType::kStack, 48},
                                true})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.GoHasNamedReturns"),
        UnorderedElementsAre(
            Pair("retfoo",
                 ArgInfo{TypeInfo{VarType::kBaseType, "int"}, {LocationType::kStack, 0}, true}),
            Pair("retbar",
                 ArgInfo{TypeInfo{VarType::kBaseType, "bool"}, {LocationType::kStack, 8}, true})));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         CreateDwarfReader(kGoServerBinaryPath, p.index));

    // func (f *http2Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte)
    // error
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("net/http.(*http2Framer).WriteDataPadded"),
        UnorderedElementsAre(
            Pair("f", ArgInfo{TypeInfo{VarType::kPointer, "*net/http.http2Framer"},
                              {LocationType::kStack, 0}}),
            Pair("streamID",
                 ArgInfo{TypeInfo{VarType::kBaseType, "uint32"}, {LocationType::kStack, 8}}),
            Pair("endStream",
                 ArgInfo{TypeInfo{VarType::kBaseType, "bool"}, {LocationType::kStack, 12}}),
            Pair("data",
                 ArgInfo{TypeInfo{VarType::kStruct, "[]uint8"}, {LocationType::kStack, 16}}),
            Pair("pad", ArgInfo{TypeInfo{VarType::kStruct, "[]uint8"}, {LocationType::kStack, 40}}),
            // The returned "error" variable has a different decl_type than the type_name.
            Pair("~r4", ArgInfo{TypeInfo{VarType::kStruct, "runtime.iface", "error"},
                                {LocationType::kStack, 64},
                                true})));
  }
}

TEST_P(DwarfReaderTest, Go1_17FunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         CreateDwarfReader(kGo1_17BinaryPath, p.index));

    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.(*Vertex).Scale"),
        UnorderedElementsAre(
            Pair("v", ArgInfo{TypeInfo{VarType::kPointer, "*main.Vertex"},
                              {LocationType::kRegister, 0, {RegisterName::kRAX}}}),
            Pair("f", ArgInfo{TypeInfo{VarType::kBaseType, "float64"},
                              {LocationType::kRegisterFP, 0, {RegisterName::kXMM0}}})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.(*Vertex).CrossScale"),
        UnorderedElementsAre(
            Pair("v", ArgInfo{TypeInfo{VarType::kPointer, "*main.Vertex"},
                              {LocationType::kRegister, 0, {RegisterName::kRAX}}}),
            Pair("v2",
                 ArgInfo{
                     TypeInfo{VarType::kStruct, "main.Vertex"},
                     {LocationType::kRegisterFP, 0, {RegisterName::kXMM0, RegisterName::kXMM1}}}),
            Pair("f", ArgInfo{TypeInfo{VarType::kBaseType, "float64"},
                              {LocationType::kRegisterFP, 16, {RegisterName::kXMM2}}})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.Vertex.Abs"),
        UnorderedElementsAre(
            Pair("v",
                 ArgInfo{
                     TypeInfo{VarType::kStruct, "main.Vertex"},
                     {LocationType::kRegisterFP, 0, {RegisterName::kXMM0, RegisterName::kXMM1}}}),
            Pair("~r0", ArgInfo{TypeInfo{VarType::kBaseType, "float64"},
                                {LocationType::kRegisterFP, 0, {RegisterName::kXMM0}},
                                true})));
    EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgInfo("main.MixedArgTypes"),
                       UnorderedElementsAre(
                           Pair("i1", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                              {LocationType::kRegister, 0, {RegisterName::kRAX}}}),
                           Pair("b1", ArgInfo{TypeInfo{VarType::kBaseType, "bool"},
                                              {LocationType::kRegister, 8, {RegisterName::kRBX}}}),
                           Pair("b2", ArgInfo{TypeInfo{VarType::kStruct, "main.BoolWrapper"},
                                              {LocationType::kRegister,
                                               16,
                                               {RegisterName::kRCX, RegisterName::kRDI,
                                                RegisterName::kRSI, RegisterName::kR8}}}),
                           Pair("i2", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                              {LocationType::kRegister, 48, {RegisterName::kR9}}}),
                           Pair("i3", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                              {LocationType::kRegister, 56, {RegisterName::kR10}}}),
                           Pair("b3", ArgInfo{TypeInfo{VarType::kBaseType, "bool"},
                                              {LocationType::kRegister, 64, {RegisterName::kR11}}}),
                           Pair("~r6", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                               {LocationType::kRegister, 0, {RegisterName::kRAX}},
                                               true}),
                           Pair("~r7", ArgInfo{TypeInfo{VarType::kStruct, "main.BoolWrapper"},
                                               {LocationType::kRegister,
                                                8,
                                                {RegisterName::kRBX, RegisterName::kRCX,
                                                 RegisterName::kRDI, RegisterName::kRSI}},
                                               true})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.GoHasNamedReturns"),
        UnorderedElementsAre(
            Pair("retfoo", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                   {LocationType::kRegister, 0, {RegisterName::kRAX}},
                                   true}),
            Pair("retbar", ArgInfo{TypeInfo{VarType::kBaseType, "bool"},
                                   {LocationType::kRegister, 8, {RegisterName::kRBX}},
                                   true})));
  }
}

TEST_P(DwarfReaderTest, GoFunctionVarLocationConsistency) {
  DwarfReaderTestParam p = GetParam();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGo1_16BinaryPath, p.index));

  // First run GetFunctionArgInfo to automatically get all arguments.
  ASSERT_OK_AND_ASSIGN(auto function_arg_locations,
                       dwarf_reader->GetFunctionArgInfo("main.MixedArgTypes"));

  // This is required so the test doesn't pass if GetFunctionArgInfo returns nothing.
  ASSERT_THAT(function_arg_locations, SizeIs(8));

  // Finally, run a consistency check between the two methods.
  for (auto& [arg_name, arg_info] : function_arg_locations) {
    ASSERT_OK_AND_ASSIGN(VarLocation location,
                         dwarf_reader->GetArgumentLocation("main.MixedArgTypes", arg_name));
    EXPECT_EQ(location, arg_info.location)
        << absl::Substitute("Argument $0 failed consistency check", arg_name);
  }
}

INSTANTIATE_TEST_SUITE_P(DwarfReaderParameterizedTest, DwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{true},
                                           DwarfReaderTestParam{false}));

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
