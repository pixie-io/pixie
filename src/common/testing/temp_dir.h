#pragma once

#include <filesystem>

#include "src/common/base/env.h"

namespace px {
namespace testing {

/**
 * A RAII-style temporary directory. The ctor creates a brand new temporary directory under the
 * system /tmp or bazel TEST_TMPDIR. It was removed in dtor.
 */
class TempDir {
 public:
  TempDir() {
    std::filesystem::path temp_dir("/tmp");
    // See https://docs.bazel.build/versions/2.0.0/test-encyclopedia.html
    auto test_temp_dir_opt = GetEnv("TEST_TMPDIR");
    if (test_temp_dir_opt.has_value()) {
      temp_dir = test_temp_dir_opt.value();
    } else {
      LOG_FIRST_N(WARNING, 1)
          << "Test environment variables not defined. Make sure you are running "
             "from repo ToT, or use bazel test instead";
    }
    temp_dir_path_ =
        temp_dir / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code ec;
    CHECK(std::filesystem::create_directory(temp_dir_path_, ec));
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(temp_dir_path_, ec);
  }

  const std::filesystem::path& path() const { return temp_dir_path_; }

 private:
  std::filesystem::path temp_dir_path_;
};

}  // namespace testing
}  // namespace px
