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

  pl::StatusOr<std::unique_ptr<ElfReader>> s = ElfReader::Create(path);
  ASSERT_OK(s);
  std::unique_ptr<ElfReader> elf_reader = s.ConsumeValueOrDie();

  {
    auto symbol_names_or = elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kAny);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names_or = elf_reader->ListSymbols("YouFind", SymbolMatchType::kAny);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names_or = elf_reader->ListSymbols("FindThis", SymbolMatchType::kAny);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), ElementsAre("CanYouFindThis"));
  }
}

TEST(ElfReaderTest, ListSymbolsExactMatch) {
  const std::string path = pl::TestEnvironment::PathToTestDataFile(kBinary);

  pl::StatusOr<std::unique_ptr<ElfReader>> s = ElfReader::Create(path);
  ASSERT_OK(s);
  std::unique_ptr<ElfReader> elf_reader = s.ConsumeValueOrDie();

  {
    auto symbol_names_or = elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kExact);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names_or = elf_reader->ListSymbols("YouFind", SymbolMatchType::kExact);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), IsEmpty());
  }

  {
    auto symbol_names_or = elf_reader->ListSymbols("FindThis", SymbolMatchType::kExact);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), IsEmpty());
  }
}

TEST(ElfReaderTest, ListSymbolsSuffixMatch) {
  const std::string path = pl::TestEnvironment::PathToTestDataFile(kBinary);

  pl::StatusOr<std::unique_ptr<ElfReader>> s = ElfReader::Create(path);
  ASSERT_OK(s);
  std::unique_ptr<ElfReader> elf_reader = s.ConsumeValueOrDie();

  {
    auto symbol_names_or = elf_reader->ListSymbols("CanYouFindThis", SymbolMatchType::kSuffix);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), ElementsAre("CanYouFindThis"));
  }

  {
    auto symbol_names_or = elf_reader->ListSymbols("YouFind", SymbolMatchType::kSuffix);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), IsEmpty());
  }

  {
    auto symbol_names_or = elf_reader->ListSymbols("FindThis", SymbolMatchType::kSuffix);
    ASSERT_OK(symbol_names_or);
    EXPECT_THAT(symbol_names_or.ValueOrDie(), ElementsAre("CanYouFindThis"));
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

  pl::StatusOr<std::unique_ptr<ElfReader>> s = ElfReader::Create(path);
  ASSERT_OK(s);
  std::unique_ptr<ElfReader> elf_reader = s.ConsumeValueOrDie();

  {
    pl::StatusOr<int64_t> s = elf_reader->SymbolAddress(symbol);
    ASSERT_OK(s);
    EXPECT_EQ(s.ValueOrDie(), expected_symbol_addr);
  }

  {
    pl::StatusOr<int64_t> s = elf_reader->SymbolAddress("bogus");
    ASSERT_OK(s);
    EXPECT_EQ(s.ValueOrDie(), -1);
  }
}
#endif
