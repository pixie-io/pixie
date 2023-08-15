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

#include <regex>

#include "src/stirling/obj_tools/dwarf_reader.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"
#include "src/stirling/utils/detect_application.h"

constexpr std::string_view kTestGo1_17Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_17_binary";
constexpr std::string_view kTestGo1_18Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_18_binary";
constexpr std::string_view kTestGo1_19Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_19_binary";
constexpr std::string_view kTestGo1_20Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_20_binary";
constexpr std::string_view kTestGo1_21Binary =
    "src/stirling/obj_tools/testdata/go/test_go_1_21_binary";
constexpr std::string_view kGoGRPCServer =
    "src/stirling/testing/demo_apps/go_grpc_tls_pl/server/golang_1_19_grpc_tls_server_binary";
constexpr std::string_view kCppBinary = "src/stirling/obj_tools/testdata/cc/test_exe_/test_exe";
constexpr std::string_view kGoBinaryUnconventional =
    "src/stirling/obj_tools/testdata/go/sockshop_payments_service";

const auto kCPPBinaryPath = px::testing::BazelRunfilePath(kCppBinary);
const auto kGo1_17BinaryPath = px::testing::BazelRunfilePath(kTestGo1_17Binary);
const auto kGo1_18BinaryPath = px::testing::BazelRunfilePath(kTestGo1_18Binary);
const auto kGo1_19BinaryPath = px::testing::BazelRunfilePath(kTestGo1_19Binary);
const auto kGo1_20BinaryPath = px::testing::BazelRunfilePath(kTestGo1_20Binary);
const auto kGo1_21BinaryPath = px::testing::BazelRunfilePath(kTestGo1_21Binary);
const auto kGoServerBinaryPath = px::testing::BazelRunfilePath(kGoGRPCServer);
const auto kGoBinaryUnconventionalPath = px::testing::BazelRunfilePath(kGoBinaryUnconventional);

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
  std::string binary_path;
  bool index;
};

auto CreateDwarfReader(const std::filesystem::path& path, bool indexing) {
  if (indexing) {
    return DwarfReader::CreateIndexingAll(path);
  }
  return DwarfReader::CreateWithoutIndexing(path);
}

class CppDwarfReaderTest : public ::testing::TestWithParam<DwarfReaderTestParam> {
 protected:
  void SetUp() override {
    DwarfReaderTestParam p = GetParam();
    ASSERT_OK_AND_ASSIGN(dwarf_reader, CreateDwarfReader(p.binary_path, p.index));
  }
  std::unique_ptr<DwarfReader> dwarf_reader;
};

class GolangDwarfReaderTest : public ::testing::TestWithParam<DwarfReaderTestParam> {
 protected:
  void SetUp() override {
    DwarfReaderTestParam p = GetParam();
    ASSERT_OK_AND_ASSIGN(dwarf_reader, CreateDwarfReader(p.binary_path, p.index));
  }

  StatusOr<SemVer> GetGoVersion() const {
    DwarfReaderTestParam p = GetParam();
    std::regex underscore("_");
    std::string path = std::regex_replace(p.binary_path, underscore, ".");
    return GetSemVer(path, false);
  }

  StatusOr<bool> UsesRegABI() const {
    constexpr SemVer kFirstRegABIVersion{.major = 1, .minor = 17, .patch = 0};
    PX_ASSIGN_OR_RETURN(SemVer go_version, GetGoVersion());
    return kFirstRegABIVersion <= go_version;
  }

  std::unique_ptr<DwarfReader> dwarf_reader;
};

class GolangDwarfReaderIndexTest : public ::testing::TestWithParam<bool> {
  std::unique_ptr<DwarfReader> dwarf_reader;
};

TEST_P(CppDwarfReaderTest, NonExistentPath) {
  auto s = DwarfReader::CreateWithoutIndexing("/bogus");
  ASSERT_NOT_OK(s);
}

TEST_P(CppDwarfReaderTest, SourceLanguage) {
  {
    // We use C++17, but the dwarf shows 14.
    EXPECT_EQ(dwarf_reader->source_language(), llvm::dwarf::DW_LANG_C_plus_plus_14);
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("clang"));
  }
}

TEST_P(GolangDwarfReaderTest, SourceLanguage) {
  {
    EXPECT_EQ(dwarf_reader->source_language(), llvm::dwarf::DW_LANG_Go);
    EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("go"));

    ASSERT_OK_AND_ASSIGN(const bool uses_regabi, UsesRegABI());

    if (uses_regabi) {
      EXPECT_THAT(dwarf_reader->compiler(), ::testing::HasSubstr("regabi"));
    } else {
      EXPECT_THAT(dwarf_reader->compiler(), ::testing::Not(::testing::HasSubstr("regabi")));
    }
  }
}

// Tests that GetMatchingDIEs() returns empty vector when nothing is found.
TEST_P(CppDwarfReaderTest, GetMatchingDIEsReturnsEmptyVector) {
  ASSERT_OK_AND_THAT(
      dwarf_reader->GetMatchingDIEs("non-existent-name", llvm::dwarf::DW_TAG_structure_type),
      IsEmpty());
}

TEST_P(CppDwarfReaderTest, GetStructByteSize) {
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("ABCStruct32"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("ABCStruct64"), 24);
}

TEST_P(GolangDwarfReaderTest, GetStructByteSize) {
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("main.Vertex"), 16);
}

TEST_P(CppDwarfReaderTest, GetStructMemberInfo) {
  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructMemberInfo("ABCStruct32", llvm::dwarf::DW_TAG_structure_type, "b",
                                        llvm::dwarf::DW_TAG_member),
      (StructMemberInfo{4, TypeInfo{VarType::kBaseType, "int"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("ABCStruct32", llvm::dwarf::DW_TAG_structure_type,
                                                  "bogus", llvm::dwarf::DW_TAG_member));
}

TEST_P(GolangDwarfReaderTest, GetStructMemberInfo) {
  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructMemberInfo("main.Vertex", llvm::dwarf::DW_TAG_structure_type, "Y",
                                        llvm::dwarf::DW_TAG_member),
      (StructMemberInfo{8, TypeInfo{VarType::kBaseType, "float64"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("main.Vertex", llvm::dwarf::DW_TAG_structure_type,
                                                  "bogus", llvm::dwarf::DW_TAG_member));
}

TEST_P(CppDwarfReaderTest, GetStructMemberOffset) {
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct32", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct32", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("ABCStruct32", "bogus"));
}
//
TEST_P(GolangDwarfReaderTest, GetStructMemberOffset) {
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("main.Vertex", "Y"), 8);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("main.Vertex", "bogus"));
}

TEST_P(CppDwarfReaderTest, GetStructSpec) {
  EXPECT_OK_AND_EQ(
      dwarf_reader->GetStructSpec("OuterStruct"),
      (std::vector{
          StructSpecEntry{.offset = 0,
                          .size = 8,
                          .type_info = {.type = VarType::kBaseType,
                                        .type_name = "long",
                                        .decl_type = "int64_t"},
                          .path = "/O0"},
          StructSpecEntry{
              .offset = 8,
              .size = 1,
              .type_info = {.type = VarType::kBaseType, .type_name = "bool", .decl_type = "bool"},
              .path = "/O1/M0/L0"},
          StructSpecEntry{
              .offset = 12,
              .size = 4,
              .type_info = {.type = VarType::kBaseType, .type_name = "int", .decl_type = "int32_t"},
              .path = "/O1/M0/L1"},
          StructSpecEntry{
              .offset = 16,
              .size = 8,
              .type_info = {.type = VarType::kPointer, .type_name = "long*", .decl_type = "long*"},
              .path = "/O1/M0/L2"},
          StructSpecEntry{
              .offset = 24,
              .size = 1,
              .type_info = {.type = VarType::kBaseType, .type_name = "bool", .decl_type = "bool"},
              .path = "/O1/M1"},
          StructSpecEntry{
              .offset = 32,
              .size = 1,
              .type_info = {.type = VarType::kBaseType, .type_name = "bool", .decl_type = "bool"},
              .path = "/O1/M2/L0"},
          StructSpecEntry{
              .offset = 36,
              .size = 4,
              .type_info = {.type = VarType::kBaseType, .type_name = "int", .decl_type = "int32_t"},
              .path = "/O1/M2/L1"},
          StructSpecEntry{
              .offset = 40,
              .size = 8,
              .type_info = {.type = VarType::kPointer, .type_name = "long*", .decl_type = "long*"},
              .path = "/O1/M2/L2"},
      }));

  EXPECT_NOT_OK(dwarf_reader->GetStructSpec("Bogus"));
}

TEST_P(GolangDwarfReaderTest, GetStructSpec) {
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

TEST_P(CppDwarfReaderTest, ArgumentTypeByteSize) {
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("CanYouFindThis", "a"), 4);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("ABCSum32", "x"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "a"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "x"), 8);
}

TEST_P(GolangDwarfReaderTest, ArgumentTypeByteSize) {
  // v is of type *Vertex.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.(*Vertex).Scale", "v"), 8);
  // f is of type float64.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.(*Vertex).Scale", "f"), 8);
  // v is of type Vertex.
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("main.Vertex.Abs", "v"), 16);
}

TEST_P(CppDwarfReaderTest, ArgumentLocation) {
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

TEST_P(GolangDwarfReaderTest, ArgumentLocation) {
  ASSERT_OK_AND_ASSIGN(bool uses_regabi, UsesRegABI());
  auto location = uses_regabi ? LocationType::kRegister : LocationType::kStack;

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "v"),
                   (VarLocation{.loc_type = location, .offset = 0}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "f"),
                   (VarLocation{.loc_type = location, .offset = uses_regabi ? 17 : 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v"),
                   (VarLocation{.loc_type = location, .offset = 0}));
  if (uses_regabi) {
    // TODO(oazizi): Support multi-piece arguments that span multiple registers.
    EXPECT_NOT_OK(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v2"));
  } else {
    EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v2"),
                     (VarLocation{.loc_type = location, .offset = 8}));
  }

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "f"),
                   (VarLocation{.loc_type = location, .offset = uses_regabi ? 19 : 24}));

  if (uses_regabi) {
    // TODO(oazizi): Support multi-piece arguments that span multiple registers.
    EXPECT_NOT_OK(dwarf_reader->GetArgumentLocation("main.Vertex.Abs", "v"));
  } else {
    EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.Vertex.Abs", "v"),
                     (VarLocation{.loc_type = location, .offset = 0}));
  }
}

// Note the differences here and the results in CppArgumentStackPointerOffset.
// This needs more investigation. Appears as though there are issues with alignment and
// also the reference point of the offset.
TEST_P(CppDwarfReaderTest, FunctionArgInfo) {
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

TEST_P(CppDwarfReaderTest, FunctionRetValInfo) {
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("CanYouFindThis"),
                   (RetValInfo{TypeInfo{VarType::kBaseType, "int"}, 4}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("ABCSum32"),
                   (RetValInfo{TypeInfo{VarType::kStruct, "ABCStruct32"}, 12}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("SomeFunctionWithPointerArgs"),
                   (RetValInfo{TypeInfo{VarType::kVoid, ""}, 0}));
}

TEST_P(GolangDwarfReaderTest, FunctionArgInfo) {
  ASSERT_OK_AND_ASSIGN(bool uses_regabi, UsesRegABI());
  {
    if (uses_regabi) {
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

      // Return values in Go1.17 start from ~r6 instead of ~r0.

      ASSERT_OK_AND_ASSIGN(SemVer go_version, GetGoVersion());
      constexpr SemVer kGoVersion1_17 = {1, 17, 0};
      std::string retval_0 = go_version == kGoVersion1_17 ? "~r6" : "~r0";
      std::string retval_1 = go_version == kGoVersion1_17 ? "~r7" : "~r1";

      EXPECT_OK_AND_THAT(
          dwarf_reader->GetFunctionArgInfo("main.MixedArgTypes"),
          UnorderedElementsAre(
              Pair("i1", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                 {LocationType::kRegister, 0, {RegisterName::kRAX}}}),
              Pair("b1", ArgInfo{TypeInfo{VarType::kBaseType, "bool"},
                                 {LocationType::kRegister, 8, {RegisterName::kRBX}}}),
              Pair("b2", ArgInfo{TypeInfo{VarType::kStruct, "main.BoolWrapper"},
                                 {LocationType::kRegister,
                                  16,
                                  {RegisterName::kRCX, RegisterName::kRDI, RegisterName::kRSI,
                                   RegisterName::kR8}}}),
              Pair("i2", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                 {LocationType::kRegister, 48, {RegisterName::kR9}}}),
              Pair("i3", ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                 {LocationType::kRegister, 56, {RegisterName::kR10}}}),
              Pair("b3", ArgInfo{TypeInfo{VarType::kBaseType, "bool"},
                                 {LocationType::kRegister, 64, {RegisterName::kR11}}}),
              Pair(retval_0, ArgInfo{TypeInfo{VarType::kBaseType, "int"},
                                     {LocationType::kRegister, 0, {RegisterName::kRAX}},
                                     true}),
              Pair(retval_1, ArgInfo{TypeInfo{VarType::kStruct, "main.BoolWrapper"},
                                     {LocationType::kRegister,
                                      8,
                                      {RegisterName::kRBX, RegisterName::kRCX, RegisterName::kRDI,
                                       RegisterName::kRSI}},
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
    } else {
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
              Pair(
                  "retbar",
                  ArgInfo{TypeInfo{VarType::kBaseType, "bool"}, {LocationType::kStack, 8}, true})));
    }
  }
}

// TODO(oazizi): This seems to work only on go1.16.
// If this is expected to work on go 1.17+, fix and re-enable. Else remove the test.
TEST_P(GolangDwarfReaderTest, DISABLED_FunctionVarLocationConsistency) {
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

// Inspired from a real life case.
TEST_P(GolangDwarfReaderIndexTest, UnconventionalGetStructMemberOffset) {
  bool index = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGoBinaryUnconventionalPath, index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("runtime.g", "goid"), 192);
}

TEST_P(GolangDwarfReaderIndexTest, FunctionArgInfo) {
  bool index = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       CreateDwarfReader(kGoServerBinaryPath, index));

  // func (f *http2Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte)
  // error
  EXPECT_OK_AND_THAT(
      dwarf_reader->GetFunctionArgInfo("net/http.(*http2Framer).WriteDataPadded"),
      UnorderedElementsAre(
          Pair("f", ArgInfo{TypeInfo{VarType::kPointer, "*net/http.http2Framer"},
                            {LocationType::kRegister, 0, {RegisterName::kRAX}}}),
          Pair("streamID", ArgInfo{TypeInfo{VarType::kBaseType, "uint32"},
                                   {LocationType::kRegister, 8, {RegisterName::kRBX}}}),
          Pair("endStream", ArgInfo{TypeInfo{VarType::kBaseType, "bool"},
                                    {LocationType::kRegister, 16, {RegisterName::kRCX}}}),
          Pair("data", ArgInfo{TypeInfo{VarType::kStruct, "[]uint8"},
                               {LocationType::kRegister,
                                24,
                                {RegisterName::kRDI, RegisterName::kRSI, RegisterName::kR8}}}),
          Pair("pad", ArgInfo{TypeInfo{VarType::kStruct, "[]uint8"},
                              {LocationType::kRegister,
                               48,
                               {RegisterName::kR9, RegisterName::kR10, RegisterName::kR11}}}),
          // The returned "error" variable has a different decl_type than the type_name.
          Pair("~r0",
               ArgInfo{TypeInfo{VarType::kStruct, "runtime.iface", "error"},
                       {LocationType::kRegister, 0, {RegisterName::kRAX, RegisterName::kRBX}},
                       true})));
}

INSTANTIATE_TEST_SUITE_P(CppDwarfReaderParameterizedTest, CppDwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{kCPPBinaryPath, true},
                                           DwarfReaderTestParam{kCPPBinaryPath, false}));

INSTANTIATE_TEST_SUITE_P(GolangDwarfReaderParameterizedTest, GolangDwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{kGo1_17BinaryPath, true},
                                           DwarfReaderTestParam{kGo1_17BinaryPath, false},
                                           DwarfReaderTestParam{kGo1_18BinaryPath, true},
                                           DwarfReaderTestParam{kGo1_18BinaryPath, false},
                                           DwarfReaderTestParam{kGo1_19BinaryPath, true},
                                           DwarfReaderTestParam{kGo1_19BinaryPath, false},
                                           DwarfReaderTestParam{kGo1_20BinaryPath, true},
                                           DwarfReaderTestParam{kGo1_20BinaryPath, false},
                                           DwarfReaderTestParam{kGo1_21BinaryPath, true},
                                           DwarfReaderTestParam{kGo1_21BinaryPath, false}));

INSTANTIATE_TEST_SUITE_P(GolangDwarfReaderParameterizedIndexTest, GolangDwarfReaderIndexTest,
                         ::testing::Values(true, false));
}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
