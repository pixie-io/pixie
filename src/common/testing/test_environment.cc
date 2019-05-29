#include "src/common/testing/test_environment.h"

#include "src/common/base/base.h"

namespace pl {

// We use this instead of C++FS since FS does not work on MacOS.
constexpr char kPathSep[] = "/";

std::string TestEnvironment::TestRunDir() {
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  const char* test_workspace = std::getenv("TEST_WORKSPACE");
  constexpr char kErrMsg[] =
      "Test environment variables not defined. Please make sure you are using bazel test.";
  CHECK(test_src_dir != nullptr) << kErrMsg;
  CHECK(test_workspace != nullptr) << kErrMsg;

  return std::string(test_src_dir) + kPathSep + std::string(test_workspace);
}

std::string TestEnvironment::PathToTestDataFile(const std::string_view& fname) {
  return TestRunDir() + kPathSep + std::string(fname);
}

}  // namespace pl
