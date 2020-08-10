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
      : kCppBinaryPath(pl::testing::TestFilePath(kCppBinary)),
        kGoBinaryPath(pl::testing::TestFilePath(kDummyGoBinary)),
        kGoServerBinaryPath(pl::testing::TestFilePath(kGoGRPCServer)),
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

TEST_F(DwarfReaderTest, GetMatchingDIEs) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath));

  std::vector<llvm::DWARFDie> dies;
  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("foo"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_variable);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("non-existent-name"), IsEmpty());

  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("ABCStruct"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("ABCStruct", llvm::dwarf::DW_TAG_member),
                     IsEmpty());

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("pl::testing::Foo::Bar", llvm::dwarf::DW_TAG_subprogram));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_subprogram);
  EXPECT_EQ(GetShortName(dies[0]), "Bar");
  EXPECT_EQ(GetLinkageName(dies[0]), "_ZNK2pl7testing3Foo3BarEi");

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("ABCStruct", llvm::dwarf::DW_TAG_structure_type));
  ASSERT_THAT(dies, SizeIs(1));
  ASSERT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);
}

TEST_P(DwarfReaderTest, GetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("ABCStruct", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("ABCStruct", "bogus"));
}

// Inspired from a real life case.
TEST_P(DwarfReaderTest, GetStructMemberOffsetUnconventional) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryUnconventionalPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("runtime.g", "goid"), 192);
}

TEST_P(DwarfReaderTest, CppArgumentTypeByteSize) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("CanYouFindThis", "a"), 4);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunction", "x"), 12);
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

TEST_P(DwarfReaderTest, CppArgumentStackPointerOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("SomeFunction", "x"), -32);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("SomeFunction", "y"), -64);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("CanYouFindThis", "a"), -4);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("CanYouFindThis", "b"), -8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("SomeFunctionWithPointerArgs", "a"),
                   -8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("SomeFunctionWithPointerArgs", "x"),
                   -16);
}

TEST_P(DwarfReaderTest, GolangArgumentStackPointerOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("main.(*Vertex).Scale", "v"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("main.(*Vertex).Scale", "f"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("main.(*Vertex).CrossScale", "v"),
                   0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("main.(*Vertex).CrossScale", "v2"),
                   8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("main.(*Vertex).CrossScale", "f"),
                   24);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentStackPointerOffset("main.Vertex.Abs", "v"), 0);
}

// Note the differences here and the results in CppArgumentStackPointerOffset.
// This needs more investigation. Appears as though there are issues with alignment and
// also the reference point of the offset.
TEST_P(DwarfReaderTest, CppFunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgInfo("CanYouFindThis"),
                     UnorderedElementsAre(Pair("a", ArgInfo{{0, VarType::kBaseType, "int"}}),
                                          Pair("b", ArgInfo{{4, VarType::kBaseType, "int"}})));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgInfo("SomeFunction"),
                     UnorderedElementsAre(Pair("x", ArgInfo{{0, VarType::kStruct, "ABCStruct"}}),
                                          Pair("y", ArgInfo{{12, VarType::kStruct, "ABCStruct"}})));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgInfo("SomeFunctionWithPointerArgs"),
                     UnorderedElementsAre(Pair("a", ArgInfo{{0, VarType::kPointer, "int"}}),
                                          Pair("x", ArgInfo{{8, VarType::kPointer, "ABCStruct"}})));
}

TEST_P(DwarfReaderTest, CppFunctionRetValInfo) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("CanYouFindThis"),
                   (RetValInfo{VarType::kBaseType, "int"}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("SomeFunction"),
                   (RetValInfo{VarType::kStruct, "ABCStruct"}));
  EXPECT_OK_AND_EQ(dwarf_reader->GetFunctionRetValInfo("SomeFunctionWithPointerArgs"),
                   (RetValInfo{VarType::kVoid, ""}));
}

std::ostream& operator<<(std::ostream& os, const ArgInfo& arg_info) {
  os << arg_info.ToString();
  return os;
}

TEST_P(DwarfReaderTest, GoFunctionArgInfo) {
  DwarfReaderTestParam p = GetParam();

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::Create(kGoBinaryPath, p.index));

    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.(*Vertex).Scale"),
        UnorderedElementsAre(Pair("v", ArgInfo{{0, VarType::kPointer, "main.Vertex"}}),
                             Pair("f", ArgInfo{{8, VarType::kBaseType, "float64"}})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.(*Vertex).CrossScale"),
        UnorderedElementsAre(Pair("v", ArgInfo{{0, VarType::kPointer, "main.Vertex"}}),
                             Pair("v2", ArgInfo{{8, VarType::kStruct, "main.Vertex"}}),
                             Pair("f", ArgInfo{{24, VarType::kBaseType, "float64"}})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.Vertex.Abs"),
        UnorderedElementsAre(Pair("v", ArgInfo{{0, VarType::kStruct, "main.Vertex"}}),
                             Pair("~r0", ArgInfo{{16, VarType::kBaseType, "float64"}, true})));
    EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgInfo("main.MixedArgTypes"),
                       UnorderedElementsAre(
                           Pair("i1", ArgInfo{{0, VarType::kBaseType, "int"}}),
                           Pair("b1", ArgInfo{{8, VarType::kBaseType, "bool"}}),
                           Pair("b2", ArgInfo{{9, VarType::kStruct, "main.BoolWrapper"}}),
                           Pair("i2", ArgInfo{{16, VarType::kBaseType, "int"}}),
                           Pair("i3", ArgInfo{{24, VarType::kBaseType, "int"}}),
                           Pair("b3", ArgInfo{{32, VarType::kBaseType, "bool"}}),
                           Pair("~r6", ArgInfo{{40, VarType::kBaseType, "int"}, true}),
                           Pair("~r7", ArgInfo{{48, VarType::kStruct, "main.BoolWrapper"}, true})));
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("main.GoHasNamedReturns"),
        UnorderedElementsAre(Pair("retfoo", ArgInfo{{0, VarType::kBaseType, "int"}, true}),
                             Pair("retbar", ArgInfo{{8, VarType::kBaseType, "bool"}, true})));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::Create(kGoServerBinaryPath, p.index));

    //   func (f *http2Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte)
    //   error
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgInfo("net/http.(*http2Framer).WriteDataPadded"),
        UnorderedElementsAre(Pair("f", ArgInfo{{0, VarType::kPointer, "net/http.http2Framer"}}),
                             Pair("streamID", ArgInfo{{8, VarType::kBaseType, "uint32"}}),
                             Pair("endStream", ArgInfo{{12, VarType::kBaseType, "bool"}}),
                             Pair("data", ArgInfo{{16, VarType::kStruct, "[]uint8"}}),
                             Pair("pad", ArgInfo{{40, VarType::kStruct, "[]uint8"}}),
                             Pair("~r4", ArgInfo{{64, VarType::kStruct, "runtime.iface"}, true})));
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
    ASSERT_OK_AND_ASSIGN(uint64_t offset, dwarf_reader->GetArgumentStackPointerOffset(
                                              "main.MixedArgTypes", arg_name));
    EXPECT_EQ(offset, arg_info.offset)
        << absl::Substitute("Argument $0 failed consistency check", arg_name);
  }
}

INSTANTIATE_TEST_SUITE_P(DwarfReaderParameterizedTest, DwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{true},
                                           DwarfReaderTestParam{false}));

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
