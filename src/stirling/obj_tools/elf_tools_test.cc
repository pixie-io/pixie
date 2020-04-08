#include "src/stirling/obj_tools/elf_tools.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

#include "src/common/exec/exec.h"

namespace pl {
namespace stirling {
namespace elf_tools {

// Path to self, since this is the object file that contains the CanYouFindThis() function above.
const std::string_view kBinary = "src/stirling/obj_tools/testdata/dummy_exe";

using ::testing::ElementsAre;
using ::testing::IsEmpty;

using pl::stirling::elf_tools::ElfReader;
using pl::stirling::elf_tools::SymbolMatchType;

TEST(ElfReaderTest, NonExistentPath) {
  auto s = pl::stirling::elf_tools::ElfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

TEST(ElfReaderTest, ListSymbolsAnyMatch) {
  const std::string path = pl::testing::TestFilePath(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  EXPECT_THAT(elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kSubstr),
              ElementsAre("CanYouFindThis"));
  EXPECT_THAT(elf_reader->ListSymbols("YouFind", SymbolMatchType::kSubstr),
              ElementsAre("CanYouFindThis"));
  EXPECT_THAT(elf_reader->ListSymbols("FindThis", SymbolMatchType::kSubstr),
              ElementsAre("CanYouFindThis"));
}

TEST(ElfReaderTest, ListSymbolsExactMatch) {
  const std::string path = pl::testing::TestFilePath(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  EXPECT_THAT(elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kExact),
              ElementsAre("CanYouFindThis"));
  EXPECT_THAT(elf_reader->ListSymbols("YouFind", SymbolMatchType::kExact), IsEmpty());
  EXPECT_THAT(elf_reader->ListSymbols("FindThis", SymbolMatchType::kExact), IsEmpty());
}

TEST(ElfReaderTest, ListSymbolsSuffixMatch) {
  const std::string path = pl::testing::TestFilePath(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  EXPECT_THAT(elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kSuffix),
              ElementsAre("CanYouFindThis"));
  EXPECT_THAT(elf_reader->ListSymbols("YouFind", SymbolMatchType::kSuffix), IsEmpty());
  EXPECT_THAT(elf_reader->ListSymbols("FindThis", SymbolMatchType::kSuffix),
              ElementsAre("CanYouFindThis"));
}

#ifdef __linux__
TEST(ElfReaderTest, SymbolAddress) {
  const std::string path = pl::testing::TestFilePath(kBinary);
  const std::string_view symbol = "CanYouFindThis";

  // Extract the address from nm as the gold standard.
  int64_t expected_symbol_addr = -1;
  std::string nm_out = pl::Exec(absl::StrCat("nm ", path)).ValueOrDie();
  std::vector<absl::string_view> nm_out_lines = absl::StrSplit(nm_out, '\n');
  for (auto& line : nm_out_lines) {
    if (line.find(symbol) != std::string::npos) {
      std::vector<absl::string_view> line_split = absl::StrSplit(line, ' ');
      ASSERT_FALSE(line_split.empty());
      expected_symbol_addr = std::stol(std::string(line_split[0]), nullptr, 16);
      break;
    }
  }
  ASSERT_NE(expected_symbol_addr, -1);

  // Actual tests of SymbolAddress begins here.

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    std::optional<int64_t> addr = elf_reader->SymbolAddress(symbol);
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr, expected_symbol_addr);
  }

  {
    std::optional<int64_t> addr = elf_reader->SymbolAddress("bogus");
    ASSERT_FALSE(addr.has_value());
  }
}
#endif

TEST(ElfReaderTest, ExternalDebugSymbols) {
  const std::string stripped_bin =
      pl::testing::TestFilePath("src/stirling/obj_tools/testdata/stripped_dummy_exe");
  const std::string debug_dir =
      pl::testing::TestFilePath("src/stirling/obj_tools/testdata/usr/lib/debug");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(stripped_bin, debug_dir));

  EXPECT_THAT(elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kExact),
              ElementsAre("CanYouFindThis"));
}

TEST(ElfReaderTest, FuncByteCode) {
  constexpr char kFuncByteCode[] =
      "\x55\x48\x89\xe5\x89\x7d\xfc\x89\x75\xf8\x8b\x45\xfc\x03\x45\xf8\x5d\xc3";
  {
    const std::string path =
        pl::testing::TestFilePath("src/stirling/obj_tools/testdata/prebuilt_dummy_exe");
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));
    // The byte code can be examined with:
    // objdump -d src/stirling/obj_tools/testdata/prebuilt_dummy_exe | grep CanYouFindThis -A 20
    ASSERT_OK_AND_EQ(elf_reader->FuncByteCode("CanYouFindThis"), kFuncByteCode);
  }
  {
    const std::string stripped_bin =
        pl::testing::TestFilePath("src/stirling/obj_tools/testdata/stripped_dummy_exe");
    const std::string debug_dir =
        pl::testing::TestFilePath("src/stirling/obj_tools/testdata/usr/lib/debug");
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                         ElfReader::Create(stripped_bin, debug_dir));
    ASSERT_OK_AND_EQ(elf_reader->FuncByteCode("CanYouFindThis"), kFuncByteCode);
  }
}

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
