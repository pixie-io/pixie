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
  const std::string path = pl::testing::BazelBinTestFilePath(kBinary);

  std::vector<llvm::DWARFDie> dies;

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader, DwarfReader::Create(path));
  ASSERT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("foo"), IsEmpty());
  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("PairStruct"));
  ASSERT_THAT(dies, SizeIs(1));
  ASSERT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);

  ASSERT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("PairStruct", llvm::dwarf::DW_TAG_member),
                     IsEmpty());
  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("PairStruct", llvm::dwarf::DW_TAG_structure_type));
  ASSERT_THAT(dies, SizeIs(1));
  ASSERT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);
}

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
