#include "src/stirling/obj_tools/dwarf_tools.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

constexpr std::string_view kDummyGoBinary =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";
constexpr std::string_view kGoGRPCServer =
    "demos/client_server_apps/go_grpc_tls_pl/server/server_/server";
constexpr std::string_view kCppBinary = "src/stirling/obj_tools/testdata/dummy_exe";
constexpr std::string_view kGoBinaryUnconventional =
    "src/stirling/obj_tools/testdata/sockshop_payments_service";

namespace pl {
namespace stirling {
namespace dwarf_tools {

using ::llvm::DWARFDie;
using ::pl::stirling::dwarf_tools::DwarfReader;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

struct DwarfReaderTestParam {
  bool index;
};

class DwarfReaderTest : public ::testing::TestWithParam<DwarfReaderTestParam> {
 protected:
  DwarfReaderTest()
      : kCppBinaryPath(pl::testing::BazelBinTestFilePath(kCppBinary)),
        kGoBinaryPath(pl::testing::BazelBinTestFilePath(kDummyGoBinary)),
        kGoServerBinaryPath(pl::testing::BazelBinTestFilePath(kGoGRPCServer)),
        kGoBinaryUnconventionalPath(pl::testing::TestFilePath(kGoBinaryUnconventional)) {}

  const std::string kCppBinaryPath;
  const std::string kGoBinaryPath;
  const std::string kGoServerBinaryPath;
  const std::string kGoBinaryUnconventionalPath;
};

TEST_F(DwarfReaderTest, NonExistentPath) {
  auto s = pl::stirling::dwarf_tools::DwarfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

TEST_F(DwarfReaderTest, SourceLanguage) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, /*index*/ true));
  // We use C++17, but the dwarf shows 14.
  EXPECT_EQ(dwarf_reader->source_language(), llvm::dwarf::DW_LANG_C_plus_plus_14);
}

TEST_F(DwarfReaderTest, GetMatchingDIEs) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath));

  std::vector<llvm::DWARFDie> dies;
  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("foo"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_variable);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("non-existent-name"), IsEmpty());

  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("ABCStruct32"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("ABCStruct32", llvm::dwarf::DW_TAG_member),
                     IsEmpty());

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("pl::testing::Foo::Bar", llvm::dwarf::DW_TAG_subprogram));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_subprogram);
  // Although the DIE does not have name attribute, DWARFDie::getShortName() walks
  // DW_AT_specification attribute to find the name.
  EXPECT_EQ(GetShortName(dies[0]), "Bar");
  EXPECT_THAT(std::string(GetLinkageName(dies[0])), ::testing::StrEq("_ZNK2pl7testing3Foo3BarEi"));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("pl::testing::Foo::Bar", "this"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("pl::testing::Foo::Bar", "i"), 4);

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("ABCStruct32", llvm::dwarf::DW_TAG_structure_type));
  ASSERT_THAT(dies, SizeIs(1));
  ASSERT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);
}

TEST_P(DwarfReaderTest, CppGetStructByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("ABCStruct32"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("ABCStruct64"), 24);
}

TEST_P(DwarfReaderTest, GolangGetStructByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructByteSize("main.Vertex"), 16);
}

TEST_P(DwarfReaderTest, CppGetStructMemberInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberInfo("ABCStruct32", "b"),
                   (StructMemberInfo{4, TypeInfo{VarType::kBaseType, "int"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("ABCStruct32", "bogus"));
}

TEST_P(DwarfReaderTest, GoGetStructMemberInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberInfo("main.Vertex", "Y"),
                   (StructMemberInfo{8, TypeInfo{VarType::kBaseType, "float64"}}));
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberInfo("main.Vertex", "bogus"));
}

TEST_P(DwarfReaderTest, CppGetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct32", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct32", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("ABCStruct32", "bogus"));
}

TEST_P(DwarfReaderTest, GoGetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("main.Vertex", "Y"), 8);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("main.Vertex", "bogus"));
}

// Inspired from a real life case.
TEST_P(DwarfReaderTest, GetStructMemberOffsetUnconventional) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryUnconventionalPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("runtime.g", "goid"), 192);
}

TEST_P(DwarfReaderTest, CppGetStructSpec) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

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
                       DwarfReader::Create(kGoBinaryPath, p.index));

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
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("CanYouFindThis", "a"), 4);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("ABCSum32", "x"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "a"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "x"), 8);
}

TEST_P(DwarfReaderTest, GolangArgumentTypeByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

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
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("ABCSum32", "x"),
                   (ArgLocation{.loc_type = LocationType::kRegister, .offset = 32}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("ABCSum32", "y"),
                   (ArgLocation{.loc_type = LocationType::kRegister, .offset = 64}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("CanYouFindThis", "a"),
                   (ArgLocation{.loc_type = LocationType::kRegister, .offset = 4}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("CanYouFindThis", "b"),
                   (ArgLocation{.loc_type = LocationType::kRegister, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("SomeFunctionWithPointerArgs", "a"),
                   (ArgLocation{.loc_type = LocationType::kRegister, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("SomeFunctionWithPointerArgs", "x"),
                   (ArgLocation{.loc_type = LocationType::kRegister, .offset = 16}));
}

TEST_P(DwarfReaderTest, GolangArgumentLocation) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "v"),
                   (ArgLocation{.loc_type = LocationType::kStack, .offset = 0}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).Scale", "f"),
                   (ArgLocation{.loc_type = LocationType::kStack, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v"),
                   (ArgLocation{.loc_type = LocationType::kStack, .offset = 0}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "v2"),
                   (ArgLocation{.loc_type = LocationType::kStack, .offset = 8}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.(*Vertex).CrossScale", "f"),
                   (ArgLocation{.loc_type = LocationType::kStack, .offset = 24}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentLocation("main.Vertex.Abs", "v"),
                   (ArgLocation{.loc_type = LocationType::kStack, .offset = 0}));
}

// Note the differences here and the results in CppArgumentStackPointerOffset.
// This needs more investigation. Appears as though there are issues with alignment and
// also the reference point of the offset.
TEST_P(DwarfReaderTest, CppFunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_THAT(
      dwarf_reader->GetFunctionArgInfo("CanYouFindThis"),
      UnorderedElementsAre(Pair("a", ArgInfo{TypeInfo{VarType::kBaseType, "int", "int"},
                                             {LocationType::kRegister, 0}}),
                           Pair("b", ArgInfo{TypeInfo{VarType::kBaseType, "int", "int"},
                                             {LocationType::kRegister, 8}})));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgInfo("ABCSum32"),
                     UnorderedElementsAre(
                         Pair("x", ArgInfo{TypeInfo{VarType::kStruct, "ABCStruct32", "ABCStruct32"},
                                           {LocationType::kRegister, 0}}),
                         Pair("y", ArgInfo{TypeInfo{VarType::kStruct, "ABCStruct32", "ABCStruct32"},
                                           {LocationType::kRegister, 16}})));
  EXPECT_OK_AND_THAT(
      dwarf_reader->GetFunctionArgInfo("SomeFunctionWithPointerArgs"),
      UnorderedElementsAre(
          Pair("a",
               ArgInfo{TypeInfo{VarType::kPointer, "int*", "int*"}, {LocationType::kRegister, 0}}),
          Pair("x", ArgInfo{TypeInfo{VarType::kPointer, "ABCStruct32*", "ABCStruct32*"},
                            {LocationType::kRegister, 8}})));
}

TEST_P(DwarfReaderTest, CppFunctionRetValInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("CanYouFindThis"),
                   (RetValInfo{TypeInfo{VarType::kBaseType, "int"}, 4}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("ABCSum32"),
                   (RetValInfo{TypeInfo{VarType::kStruct, "ABCStruct32"}, 12}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("SomeFunctionWithPointerArgs"),
                   (RetValInfo{TypeInfo{VarType::kVoid, ""}, 0}));
}

TEST_P(DwarfReaderTest, GoFunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::Create(kGoBinaryPath, p.index));

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
                         DwarfReader::Create(kGoServerBinaryPath, p.index));

    //   func (f *http2Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte)
    //   error
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

TEST_P(DwarfReaderTest, GoFunctionArgLocationConsistency) {
  DwarfReaderTestParam p = GetParam();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  // First run GetFunctionArgInfo to automatically get all arguments.
  ASSERT_OK_AND_ASSIGN(auto function_arg_locations,
                       dwarf_reader->GetFunctionArgInfo("main.MixedArgTypes"));

  // This is required so the test doesn't pass if GetFunctionArgInfo returns nothing.
  ASSERT_THAT(function_arg_locations, SizeIs(8));

  // Finally, run a consistency check between the two methods.
  for (auto& [arg_name, arg_info] : function_arg_locations) {
    ASSERT_OK_AND_ASSIGN(ArgLocation location,
                         dwarf_reader->GetArgumentLocation("main.MixedArgTypes", arg_name));
    EXPECT_EQ(location, arg_info.location)
        << absl::Substitute("Argument $0 failed consistency check", arg_name);
  }
}

INSTANTIATE_TEST_SUITE_P(DwarfReaderParameterizedTest, DwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{true},
                                           DwarfReaderTestParam{false}));

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
