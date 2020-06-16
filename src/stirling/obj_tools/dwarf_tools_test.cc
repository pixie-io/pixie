#include "src/stirling/obj_tools/dwarf_tools.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace dwarf_tools {

const std::string_view kBinary = "src/stirling/obj_tools/testdata/prebuilt_dummy_exe";

using ::pl::stirling::dwarf_tools::DwarfReader;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(DwarfReaderTest, NonExistentPath) {
  auto s = pl::stirling::dwarf_tools::DwarfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

TEST(DwarfReaderTest, Basic) {
  const std::string path = pl::testing::TestFilePath(kBinary);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader, DwarfReader::Create(path));

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

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("PairStruct", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("PairStruct", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("PairStruct", "bogus"));
}

TEST(DwarfReaderTest, WithoutIndex) {
  const std::string path = pl::testing::TestFilePath(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(path, /* index */ false));

  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("PairStruct", "a"), 0);
  EXPECT_OK_AND_EQ(dwarf_reader->GetStructMemberOffset("PairStruct", "b"), 4);
  EXPECT_NOT_OK(dwarf_reader->GetStructMemberOffset("PairStruct", "bogus"));
}

TEST(DwarfReaderTest, ArgumentTypeByteSize) {
  const std::string path = pl::testing::TestFilePath(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::Create(path, /* index */ false));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("CanYouFindThis", "a"), 4);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunction", "x"), 12);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "a"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("SomeFunctionWithPointerArgs", "x"), 8);
}

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
