#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "src/common/testing/test_environment.h"

namespace pl {
namespace stirling {

TEST(StirlingWrapperSizeTest, ExecutableSizeIsLessThan100MiB) {
  const std::string stirling_wrapper_path =
      TestEnvironment::PathToTestDataFile("src/stirling/stirling_wrapper");
  EXPECT_LE(std::filesystem::file_size(std::filesystem::path(stirling_wrapper_path)),
            100 * 1024 * 1024);
}

}  // namespace stirling
}  // namespace pl
