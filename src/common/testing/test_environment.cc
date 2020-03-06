#include "src/common/testing/test_environment.h"

#include "src/common/base/base.h"

namespace pl {
namespace testing {

namespace {

std::filesystem::path TestBaseDir() {
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  const char* test_workspace = std::getenv("TEST_WORKSPACE");

  if (test_src_dir == nullptr || test_workspace == nullptr) {
    LOG_FIRST_N(WARNING, 1) << "Test environment variables not defined. Make sure you are running "
                               "from repo ToT, or use bazel test instead";
    // Return PWD. User has been warned that test will only run properly when run from ToT.
    return {};
  }
  return std::filesystem::path(test_src_dir) / test_workspace;
}

}  // namespace

std::filesystem::path TestFilePath(const std::filesystem::path& rel_path) {
  CHECK(rel_path.is_relative());
  return TestBaseDir() / rel_path;
}

}  // namespace testing
}  // namespace pl
