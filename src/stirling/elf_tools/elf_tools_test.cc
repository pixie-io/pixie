#include "src/stirling/elf_tools/elf_tools.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

#include "src/common/exec/exec.h"

// Path to self, since this is the object file that contains the CanYouFindThis() function above.
const std::string_view kBinary = "src/stirling/elf_tools/testdata/dummy_exe";

using ::testing::ElementsAre;
using ::testing::IsEmpty;

using pl::stirling::elf_tools::ElfReader;
using pl::stirling::elf_tools::SymbolMatchType;

TEST(ElfReaderTest, NonExistentPath) {
  auto s = pl::stirling::elf_tools::ElfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

TEST(ElfReaderTest, ListSymbolsAnyMatch) {
  const std::string path = pl::TestEnvironment::PathToTestDataFile(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    auto symbol_names = elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kAny);
    EXPECT_OK_AND_THAT(symbol_names, ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names = elf_reader->ListSymbols("YouFind", SymbolMatchType::kAny);
    EXPECT_OK_AND_THAT(symbol_names, ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names = elf_reader->ListSymbols("FindThis", SymbolMatchType::kAny);
    EXPECT_OK_AND_THAT(symbol_names, ElementsAre("CanYouFindThis"));
  }
}

TEST(ElfReaderTest, ListSymbolsExactMatch) {
  const std::string path = pl::TestEnvironment::PathToTestDataFile(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    auto symbol_names = elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kExact);
    EXPECT_OK_AND_THAT(symbol_names, ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names = elf_reader->ListSymbols("YouFind", SymbolMatchType::kExact);
    EXPECT_OK_AND_THAT(symbol_names, IsEmpty());
  }

  {
    auto symbol_names = elf_reader->ListSymbols("FindThis", SymbolMatchType::kExact);
    EXPECT_OK_AND_THAT(symbol_names, IsEmpty());
  }
}

TEST(ElfReaderTest, ListSymbolsSuffixMatch) {
  const std::string path = pl::TestEnvironment::PathToTestDataFile(kBinary);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    auto symbol_names = elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kSuffix);
    EXPECT_OK_AND_THAT(symbol_names, ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names = elf_reader->ListSymbols("YouFind", SymbolMatchType::kSuffix);
    EXPECT_OK_AND_THAT(symbol_names, IsEmpty());
  }

  {
    auto symbol_names = elf_reader->ListSymbols("FindThis", SymbolMatchType::kSuffix);
    EXPECT_OK_AND_THAT(symbol_names, ElementsAre("CanYouFindThis"));
  }
}

#ifdef __linux__
TEST(ElfReaderTest, SymbolAddress) {
  const std::string path = pl::TestEnvironment::PathToTestDataFile(kBinary);
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
  EXPECT_OK_AND_EQ(elf_reader->SymbolAddress(symbol), expected_symbol_addr);
  EXPECT_OK_AND_EQ(elf_reader->SymbolAddress("bogus"), -1);
}
#endif
