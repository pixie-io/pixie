#include "src/stirling/obj_tools/dwarf_tools.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

// The go binary location cannot be hard-coded because its location changes based on
// -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");
const std::string_view kCppBinary = "src/stirling/obj_tools/testdata/prebuilt_dummy_exe";

namespace pl {
namespace stirling {
namespace dwarf_tools {

using ::pl::stirling::dwarf_tools::DwarfReader;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

struct DwarfReaderTestParam {
  bool index;
};

class DwarfReaderTest : public ::testing::TestWithParam<DwarfReaderTestParam> {
 protected:
  DwarfReaderTest()
      : kCppBinaryPath(pl::testing::TestFilePath(kCppBinary)),
        kGoBinaryPath(pl::testing::TestFilePath(FLAGS_dummy_go_binary)) {}
  const std::string kCppBinaryPath;
  const std::string kGoBinaryPath;
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

bool operator==(const FunctionArgLocation& a, const FunctionArgLocation& b) {
  return a.name == b.name && a.offset == b.offset;
}

// Note the differences here and the results in CppArgumentStackPointerOffset.
// This needs more investigation. Appears as though there are issues with alignment and
// also the reference point of the offset.
TEST_P(DwarfReaderTest, CppFunctionArgOffsets) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kCppBinaryPath, p.index));

  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("SomeFunction"),
                     ElementsAre(FunctionArgLocation{"x", 0}, FunctionArgLocation{"y", 12}));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("CanYouFindThis"),
                     ElementsAre(FunctionArgLocation{"a", 0}, FunctionArgLocation{"b", 4}));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("SomeFunctionWithPointerArgs"),
                     ElementsAre(FunctionArgLocation{"a", 0}, FunctionArgLocation{"x", 8}));
}

TEST_P(DwarfReaderTest, GoFunctionArgOffsets) {
  DwarfReaderTestParam p = GetParam();
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(kGoBinaryPath, p.index));

  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("main.(*Vertex).Scale"),
                     ElementsAre(FunctionArgLocation{"v", 0}, FunctionArgLocation{"f", 8}));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("main.(*Vertex).CrossScale"),
                     ElementsAre(FunctionArgLocation{"v", 0}, FunctionArgLocation{"v2", 8},
                                 FunctionArgLocation{"f", 24}));
  EXPECT_OK_AND_THAT(dwarf_reader->GetFunctionArgOffsets("main.Vertex.Abs"),
                     ElementsAre(FunctionArgLocation{"v", 0}, FunctionArgLocation{"~r0", 16}));
}

INSTANTIATE_TEST_SUITE_P(DwarfReaderParameterizedTest, DwarfReaderTest,
                         ::testing::Values(DwarfReaderTestParam{true},
                                           DwarfReaderTestParam{false}));

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
