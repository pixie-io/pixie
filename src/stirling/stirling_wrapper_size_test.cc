#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"

namespace pl {
namespace stirling {

#ifdef __OPTIMIZE__
constexpr uint64_t kFileSizeLimitMB = 100;
#else
constexpr uint64_t kFileSizeLimitMB = 325;
#endif

TEST(StirlingWrapperSizeTest, ExecutableSizeLimit) {
  LOG(INFO) << absl::Substitute("Size limit = $0 MB", kFileSizeLimitMB);
  const std::string stirling_wrapper_path = testing::TestFilePath("src/stirling/stirling_wrapper");

  EXPECT_LE(std::filesystem::file_size(std::filesystem::path(stirling_wrapper_path)),
            kFileSizeLimitMB * 1024 * 1024);
}

}  // namespace stirling
}  // namespace pl
