#include "src/stirling/obj_tools/dwarf_tools.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

// The go binary location cannot be hard-coded because its location changes based on
// -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");
DEFINE_string(go_grpc_server, "", "The path to server.");
const std::string_view kCppBinary = "src/stirling/obj_tools/testdata/prebuilt_dummy_exe";

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
        kGoBinaryPath(pl::testing::TestFilePath(FLAGS_dummy_go_binary)),
        kGoServerBinaryPath(pl::testing::TestFilePath(FLAGS_go_grpc_server)) {}
  const std::string kCppBinaryPath;
  const std::string kGoBinaryPath;
  const std::string kGoServerBinaryPath;
};

TEST_F(DwarfReaderTest, NonExistentPath) {
  auto s = pl::stirling::dwarf_tools::DwarfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

TEST_F(DwarfReaderTest, GetMatchingDIEs) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath));

  std::vector<llvm::DWARFDie> dies;

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("foo"), IsEmpty());

  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("PairStruct"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("PairStruct", llvm::dwarf::DW_TAG_member),
                     IsEmpty());

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("PairStruct", llvm::dwarf::DW_TAG_structure_type));
  ASSERT_THAT(dies, SizeIs(1));
  ASSERT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);
}

TEST_P(DwarfReaderTest, GetStructMemberOffset) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("PairStruct", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("PairStruct", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("PairStruct", "bogus"));
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
TEST_P(DwarfReaderTest, CppFunctionArgOffsets) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("SomeFunction"),
                     UnorderedElementsAre(Pair("x", 0), Pair("y", 12)));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("CanYouFindThis"),
                     UnorderedElementsAre(Pair("a", 0), Pair("b", 4)));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("SomeFunctionWithPointerArgs"),
                     UnorderedElementsAre(Pair("a", 0), Pair("x", 8)));
}

TEST_P(DwarfReaderTest, GoFunctionArgOffsets) {
  DwarfReaderTestParam p = GetParam();

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::Create(kGoBinaryPath, p.index));

    EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("main.(*Vertex).Scale"),
                       UnorderedElementsAre(Pair("v", 0), Pair("f", 8)));
    EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("main.(*Vertex).CrossScale"),
                       UnorderedElementsAre(Pair("v", 0), Pair("v2", 8), Pair("f", 24)));
    EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("main.Vertex.Abs"),
                       UnorderedElementsAre(Pair("v", 0), Pair("~r0", 16)));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                         DwarfReader::Create(kGoServerBinaryPath, p.index));

    //   func (f *http2Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte)
    //   error
    EXPECT_OK_AND_THAT(
        dwarf_reader->GetFunctionArgOffsets("net/http.(*http2Framer).WriteDataPadded"),
        UnorderedElementsAre(Pair("f", 0), Pair("streamID", 8), Pair("endStream", 12),
                             Pair("data", 16), Pair("pad", 40), Pair("~r4", 64)));
  }
}

TEST_P(DwarfReaderTest, GoFunctionArgConsistency) {
  DwarfReaderTestParam p = GetParam();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  // First run GetFunctionArgOffsets to automatically get all arguments.
  ASSERT_OK_AND_ASSIGN(auto function_arg_locations,
                       dwarf_reader->GetFunctionArgOffsets("main.MixedArgTypes"));

  // This is required so the test doesn't pass if GetFunctionArgOffsets returns nothing.
  ASSERT_THAT(function_arg_locations, SizeIs(7));

  // Finally, run a consistency check between the two methods.
  for (auto& [arg_name, arg_offset] : function_arg_locations) {
    ASSERT_OK_AND_ASSIGN(uint64_t offset, dwarf_reader->GetArgumentStackPointerOffset(
                                              "main.MixedArgTypes", arg_name));
    EXPECT_EQ(offset, arg_offset) << absl::Substitute("Argument $0 failed consistency check",
                                                      arg_name);
  }
}

INSTANTIATE_TEST_SUITE_P(DwarfReaderParameterizedTest, DwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{true},
                                           DwarfReaderTestParam{false}));

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
