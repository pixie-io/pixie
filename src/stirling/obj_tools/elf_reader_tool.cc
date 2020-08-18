#include <memory>

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/elf_tools.h"

DEFINE_string(binary, "", "Filename to list symbols");
DEFINE_string(filter, "", "Symbol matching substring used to select symbols to print.");

using ::pl::stirling::elf_tools::ElfReader;
using ::pl::stirling::elf_tools::SymbolMatchType;

constexpr char kProgramDescription[] =
    "A tool that lists all the function symbols in a binary (similar to nm).";

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kProgramDescription);
  pl::EnvironmentGuard env_guard(&argc, argv);

  if (FLAGS_binary.empty()) {
    LOG(INFO) << absl::Substitute(
        "Usage: $0 --binary <binary_path> [--filter <symbol matching substring>]", argv[0]);
    return 1;
  }

  LOG(INFO) << absl::Substitute("Reading symbols in $0", FLAGS_binary);

  PL_ASSIGN_OR_EXIT(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(FLAGS_binary));
  PL_ASSIGN_OR_EXIT(std::vector<ElfReader::SymbolInfo> symbol_infos,
                    elf_reader->ListFuncSymbols(FLAGS_filter, SymbolMatchType::kSubstr));

  LOG(INFO) << absl::Substitute("Found $0 symbols", symbol_infos.size());

  for (auto& s : symbol_infos) {
    LOG(INFO) << s.name;
  }
}
