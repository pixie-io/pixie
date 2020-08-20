#include "src/stirling/obj_tools/elf_tools.h"

#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/testdata/dummy_exe_fixture.h"

namespace pl {
namespace stirling {
namespace elf_tools {

const DummyExeFixture kDummyExeFixture;

using ::pl::stirling::elf_tools::ElfReader;
using ::pl::stirling::elf_tools::SymbolMatchType;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

TEST(ElfReaderTest, NonExistentPath) {
  auto s = pl::stirling::elf_tools::ElfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

auto SymbolNameIs(const std::string& n) { return Field(&ElfReader::SymbolInfo::name, n); }

TEST(ElfReaderTest, ListSymbolsAnyMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kDummyExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kSubstr),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kSubstr),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("FindThis", SymbolMatchType::kSubstr),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, ListSymbolsExactMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kDummyExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kExact), IsEmpty());
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("FindThis", SymbolMatchType::kExact), IsEmpty());
}

TEST(ElfReaderTest, ListSymbolsPrefixMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kDummyExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kPrefix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kPrefix), IsEmpty());
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYou", SymbolMatchType::kPrefix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, ListSymbolsSuffixMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kDummyExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kSuffix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kSuffix), IsEmpty());
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("FindThis", SymbolMatchType::kSuffix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

#ifdef __linux__
TEST(ElfReaderTest, SymbolAddress) {
  const std::string path = kDummyExeFixture.Path().string();
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

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, FuncByteCode) {
  {
    const std::string path =
        pl::testing::TestFilePath("src/stirling/obj_tools/testdata/prebuilt_dummy_exe");
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));
    ASSERT_OK_AND_ASSIGN(const std::vector<ElfReader::SymbolInfo> symbol_infos,
                         elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact));
    ASSERT_THAT(symbol_infos, SizeIs(1));
    const auto& symbol_info = symbol_infos.front();
    // The byte code can be examined with:
    // objdump -d src/stirling/obj_tools/testdata/prebuilt_dummy_exe | grep CanYouFindThis -A 20
    // 0x201101 is the address of the 'c3' (retq) opcode.
    ASSERT_OK_AND_THAT(elf_reader->FuncRetInstAddrs(symbol_info), ElementsAre(0x4011e1));
  }
  {
    const std::string stripped_bin =
        pl::testing::TestFilePath("src/stirling/obj_tools/testdata/stripped_dummy_exe");
    const std::string debug_dir =
        pl::testing::TestFilePath("src/stirling/obj_tools/testdata/usr/lib/debug");
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                         ElfReader::Create(stripped_bin, debug_dir));
    ASSERT_OK_AND_ASSIGN(const std::vector<ElfReader::SymbolInfo> symbol_infos,
                         elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact));
    ASSERT_THAT(symbol_infos, SizeIs(1));
    const auto& symbol_info = symbol_infos.front();
    ASSERT_OK_AND_THAT(elf_reader->FuncRetInstAddrs(symbol_info), ElementsAre(0x201101));
  }
}

TEST(ElfGolangItableTest, DISABLED_ExtractInterfaceTypes) {
  using InterfaceMap = absl::flat_hash_map<std::string, std::vector<std::string>>;

  const std::string kPath = pl::testing::BazelBinTestFilePath(
      "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(InterfaceMap interfaces, ExtractGolangInterfaces(elf_reader.get()));

  EXPECT_THAT(
      interfaces,
      UnorderedElementsAre(
          Pair("error", UnorderedElementsAre("*errors.errorString", "*os.PathError",
                                             "*internal/poll.TimeoutError", "runtime.errorString",
                                             "syscall.Errno")),
          Pair("sort.Interface", UnorderedElementsAre("*internal/fmtsort.SortedMap")),
          Pair("math/rand.Source",
               UnorderedElementsAre("*math/rand.lockedSource", "*math/rand.rngSource")),
          Pair("io.Writer", UnorderedElementsAre("*os.File")),
          Pair("internal/reflectlite.Type", UnorderedElementsAre("*internal/reflectlite.rtype")),
          Pair("reflect.Type", UnorderedElementsAre("*reflect.rtype")),
          Pair("fmt.State", UnorderedElementsAre("*fmt.pp"))));
}

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
