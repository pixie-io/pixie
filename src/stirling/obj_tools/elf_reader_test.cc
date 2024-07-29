/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/obj_tools/elf_reader.h"

#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/testdata/cc/test_exe_fixture.h"

namespace px {
namespace stirling {
namespace obj_tools {

const TestExeFixture kTestExeFixture;

using ::px::stirling::obj_tools::ElfReader;
using ::px::stirling::obj_tools::SymbolMatchType;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using ::px::operator<<;

// Models ELF section output information from objdump -h.
// ELFIO::section's do not contain virtual memory addresses like ELFIO::segment's do
// so this struct is used to store the information from objdump.
// Example objdump output:
//
// $ objdump -j .bss -h bazel-bin/src/stirling/obj_tools/testdata/cc/test_exe/test_exe
//
// bazel-bin/src/stirling/obj_tools/testdata/cc/test_exe/test_exe:     file format elf64-x86-64
//
// Sections:
// Idx Name          Size      VMA               LMA               File off  Algn
//  27 .bss          00002068  00000000000bd100  00000000000bd100  000ba100  2**5
//                    ALLOC
struct Section {
  std::string name;
  int64_t size;
  int64_t vma;
  int64_t lma;
  int64_t file_offset;
};

StatusOr<Section> ObjdumpSectionNameToAddr(const std::string& path, const std::string& section_name) {
  Section section;
  std::string objdump_out = px::Exec(absl::StrCat("objdump -h -j ", section_name, " ", path)).ValueOrDie();
  std::vector<absl::string_view> objdump_out_lines = absl::StrSplit(objdump_out, '\n');
  for (auto& line : objdump_out_lines) {
    if (line.find(section_name) != std::string::npos) {
      std::vector<absl::string_view> line_split = absl::StrSplit(line, ' ', absl::SkipWhitespace());
      CHECK(!line_split.empty());

      section.name = std::string(line_split[1]);
      section.size = std::stol(std::string(line_split[2]), nullptr, 16);
      section.vma = std::stol(std::string(line_split[3]), nullptr, 16);
      section.lma = std::stol(std::string(line_split[4]), nullptr, 16);
      section.file_offset = std::stol(std::string(line_split[5]), nullptr, 16);
      break;
    }
  }

  if (section.name != section_name) {
    return error::Internal("Unable to find section with name $0", section_name);
  }

  LOG(INFO) << "Section: " << absl::Substitute("name: $0, size: $1, vma: $2, lma: $3, file_offset: $4",
                                              section.name, section.size, section.vma, section.lma, section.file_offset);
  return section;
}

StatusOr<int64_t> NmSymbolNameToAddr(const std::string& path, const std::string& symbol_name) {
  // Extract the address from nm as the gold standard.
  int64_t symbol_addr = -1;
  std::string nm_out = px::Exec(absl::StrCat("nm ", path)).ValueOrDie();
  std::vector<absl::string_view> nm_out_lines = absl::StrSplit(nm_out, '\n');
  for (auto& line : nm_out_lines) {
    if (line.find(symbol_name) != std::string::npos) {
      std::vector<absl::string_view> line_split = absl::StrSplit(line, ' ');
      CHECK(!line_split.empty());
      symbol_addr = std::stol(std::string(line_split[0]), nullptr, 16);
      break;
    }
  }

  if (symbol_addr == -1) {
    return error::Internal("Unexpected symbol address");
  }

  return symbol_addr;
}

TEST(ElfReaderTest, NonExistentPath) {
  auto s = px::stirling::obj_tools::ElfReader::Create("/bogus");
  ASSERT_NOT_OK(s);
}

auto SymbolNameIs(const std::string& n) { return Field(&ElfReader::SymbolInfo::name, n); }

TEST(ElfReaderTest, ListFuncSymbolsAnyMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kTestExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kSubstr),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kSubstr),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("FindThis", SymbolMatchType::kSubstr),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, ListFuncSymbolsExactMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kTestExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kExact), IsEmpty());
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("FindThis", SymbolMatchType::kExact), IsEmpty());
}

TEST(ElfReaderTest, ListFuncSymbolsPrefixMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kTestExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kPrefix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kPrefix), IsEmpty());
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYou", SymbolMatchType::kPrefix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, ListFuncSymbolsSuffixMatch) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(kTestExeFixture.Path()));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kSuffix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("YouFind", SymbolMatchType::kSuffix), IsEmpty());
  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("FindThis", SymbolMatchType::kSuffix),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, SymbolAddress) {
  const std::string path = kTestExeFixture.Path().string();
  const std::string kSymbolName = "CanYouFindThis";
  ASSERT_OK_AND_ASSIGN(const int64_t symbol_addr, NmSymbolNameToAddr(path, kSymbolName));

  // Actual tests of SymbolAddress begins here.

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    std::optional<int64_t> addr = elf_reader->SymbolAddress(kSymbolName);
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr, symbol_addr);
  }

  {
    std::optional<int64_t> addr = elf_reader->SymbolAddress("bogus");
    ASSERT_FALSE(addr.has_value());
  }
}

TEST(ElfReaderTest, VirtualAddrToBinaryAddr) {
  const std::string path = kTestExeFixture.Path().string();
  const std::string kBssSection = ".data";
  ASSERT_OK_AND_ASSIGN(const Section section, ObjdumpSectionNameToAddr(path, kBssSection));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));
  const int64_t offset = 1;
  ASSERT_OK_AND_ASSIGN(auto binary_addr, elf_reader->VirtualAddrToBinaryAddr(section.vma + offset));
  EXPECT_EQ(binary_addr, section.file_offset + offset);
}

TEST(ElfReaderTest, AddrToSymbol) {
  const std::string path = kTestExeFixture.Path().string();
  const std::string kSymbolName = "CanYouFindThis";
  ASSERT_OK_AND_ASSIGN(const int64_t symbol_addr, NmSymbolNameToAddr(path, kSymbolName));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    ASSERT_OK_AND_ASSIGN(std::optional<std::string> symbol_name,
                         elf_reader->AddrToSymbol(symbol_addr));
    EXPECT_EQ(symbol_name.value_or("-"), kSymbolName);
  }

  // An address that doesn't exactly match with the symbol returns std::nullopt.
  {
    ASSERT_OK_AND_ASSIGN(std::optional<std::string> symbol_name,
                         elf_reader->AddrToSymbol(symbol_addr + 4));
    EXPECT_EQ(symbol_name.value_or("-"), "-");
  }
}

TEST(ElfReaderTest, InstrAddrToSymbol) {
  const std::string path = kTestExeFixture.Path().string();
  const std::string kSymbolName = "CanYouFindThis";
  ASSERT_OK_AND_ASSIGN(const int64_t kSymbolAddr, NmSymbolNameToAddr(path, kSymbolName));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));

  {
    ASSERT_OK_AND_ASSIGN(std::optional<std::string> symbol_name,
                         elf_reader->InstrAddrToSymbol(kSymbolAddr));
    EXPECT_EQ(symbol_name.value_or("-"), kSymbolName);
  }

  // Read an instruction a few bytes away. This should still be part of the same function.
  {
    ASSERT_OK_AND_ASSIGN(std::optional<std::string> symbol_name,
                         elf_reader->InstrAddrToSymbol(kSymbolAddr + 4));
    EXPECT_EQ(symbol_name.value_or("-"), kSymbolName);
  }

  // Read an instruction far away. This should be part of another function.
  {
    ASSERT_OK_AND_ASSIGN(std::optional<std::string> symbol_name,
                         elf_reader->InstrAddrToSymbol(kSymbolAddr + 1000));
    EXPECT_NE(symbol_name.value_or("-"), kSymbolName);
  }
}

TEST(ElfReaderTest, ExternalDebugSymbolsBuildID) {
  const std::string stripped_bin =
      px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/stripped_test_exe");
  const std::string debug_dir =
      px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/usr/lib/debug");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(stripped_bin, debug_dir));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, ExternalDebugSymbolsDebugLink) {
  const std::string stripped_bin =
      px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/test_exe_debuglink");
  const std::string debug_dir =
      px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/usr/lib/debug2");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                       ElfReader::Create(stripped_bin, debug_dir));

  EXPECT_OK_AND_THAT(elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact),
                     ElementsAre(SymbolNameIs("CanYouFindThis")));
}

TEST(ElfReaderTest, FuncByteCode) {
  {
    const std::string path =
        px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/prebuilt_test_exe");
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(path));
    ASSERT_OK_AND_ASSIGN(const std::vector<ElfReader::SymbolInfo> symbol_infos,
                         elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact));
    ASSERT_THAT(symbol_infos, SizeIs(1));
    const auto& symbol_info = symbol_infos.front();
    // The byte code can be examined with:
    // objdump -d src/stirling/obj_tools/testdata/cc/prebuilt_test_exe | grep CanYouFindThis -A 20
    // 0x201101 is the address of the 'c3' (retq) opcode.
    ASSERT_OK_AND_THAT(elf_reader->FuncRetInstAddrs(symbol_info), ElementsAre(0x4011e1));
  }
  {
    const std::string stripped_bin =
        px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/stripped_test_exe");
    const std::string debug_dir =
        px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/usr/lib/debug");
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader,
                         ElfReader::Create(stripped_bin, debug_dir));
    ASSERT_OK_AND_ASSIGN(const std::vector<ElfReader::SymbolInfo> symbol_infos,
                         elf_reader->ListFuncSymbols("CanYouFindThis", SymbolMatchType::kExact));
    ASSERT_THAT(symbol_infos, SizeIs(1));
    const auto& symbol_info = symbol_infos.front();
    ASSERT_OK_AND_THAT(elf_reader->FuncRetInstAddrs(symbol_info), ElementsAre(0x201101));
  }
}

TEST(ElfReaderTest, GolangAppRuntimeBuildVersion) {
  const std::string kPath =
      px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/go/test_go_1_19_binary");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(ElfReader::SymbolInfo symbol,
                       elf_reader->SearchTheOnlySymbol("runtime.buildVersion"));
// Coverage build might alter the resultant binary.
#ifndef PL_COVERAGE
  ASSERT_OK_AND_ASSIGN(auto expected_addr, NmSymbolNameToAddr(kPath, "runtime.buildVersion"));
  EXPECT_EQ(symbol.address, expected_addr);
#endif
  EXPECT_EQ(symbol.size, 16) << "Symbol table entry size should be 16";
  EXPECT_EQ(symbol.type, ELFIO::STT_OBJECT);
}

// Tests that the versioned symbol names always include version strings.
TEST(ElfReaderTest, VersionedSymbolsInDynamicLibrary) {
  const std::string kPath =
      px::testing::BazelRunfilePath("src/stirling/obj_tools/testdata/cc/lib_foo_so");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_THAT(elf_reader->SearchSymbols("foo", SymbolMatchType::kSubstr),
                     UnorderedElementsAre(SymbolNameIs("lib_foo.c"), SymbolNameIs("foo_new"),
                                          SymbolNameIs("foo_old"),
                                          // @@ refers to the default version.
                                          SymbolNameIs("foo@@VER_2"), SymbolNameIs("foo@VER_1")));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
