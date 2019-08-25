#include "src/common/testing/test_environment.h"

#include "src/common/base/base.h"

namespace pl {

// We use this instead of C++FS since FS does not work on MacOS.
constexpr char kPathSep[] = "/";

std::string TestEnvironment::TestRunDir() {
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  const char* test_workspace = std::getenv("TEST_WORKSPACE");

  if (test_src_dir == nullptr || test_workspace == nullptr) {
    LOG_FIRST_N(WARNING, 1) << "Test environment variables not defined. Make sure you are running "
                               "from repo ToT, or use bazel test instead";

    // Return PWD. User has been warned that test will only run properly when run from ToT.
    return std::string(".");
  }
  return std::string(test_src_dir) + kPathSep + std::string(test_workspace);
}

std::string TestEnvironment::PathToTestDataFile(const std::string_view& fname) {
  return TestRunDir() + kPathSep + std::string(fname);
}

}  // namespace pl
