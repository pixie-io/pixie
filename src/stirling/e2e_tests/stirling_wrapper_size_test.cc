#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"

namespace px {
namespace stirling {

#ifdef __OPTIMIZE__
constexpr uint64_t kFileSizeLimitMB = 90;
#else
constexpr uint64_t kFileSizeLimitMB = 250;
#endif

TEST(StirlingWrapperSizeTest, ExecutableSizeLimit) {
  LOG(INFO) << absl::Substitute("Size limit = $0 MB", kFileSizeLimitMB);
  const std::string stirling_wrapper_path =
      testing::TestFilePath("src/stirling/binaries/stirling_wrapper_core");

  EXPECT_LE(std::filesystem::file_size(std::filesystem::path(stirling_wrapper_path)),
            kFileSizeLimitMB * 1024 * 1024);
}

}  // namespace stirling
}  // namespace px
