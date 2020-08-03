#pragma once

#include <filesystem>

#include "src/common/testing/testing.h"
#include "src/common/exec/exec.h"

namespace pl {
namespace stirling {
namespace elf_tools {

// Holds a reference to the :dummy_exe, so that it's easier for tests to invoke the binary.
class DummyExeFixture {
 public:
  static constexpr char kDummyExePath[] = "src/stirling/obj_tools/testdata/dummy_exe";

  const std::filesystem::path& Path() const {
    return dummy_exe_path_;
  }

  Status Run() const {
    auto stdout_or = Exec(dummy_exe_path_);
    return stdout_or.status();
  }

 private:
  const std::filesystem::path dummy_exe_path_ = testing::BazelBinTestFilePath(kDummyExePath);
};

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
