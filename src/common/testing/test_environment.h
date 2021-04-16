#pragma once

#include <filesystem>
#include <string>

namespace px {
namespace testing {

/**
 * Returns whether the test is running through bazel or not.
 */
bool IsBazelEnvironment();

/**
 * Returns the path to a static test file, specified by a path relative to ToT.
 * Path is valid when run through bazel, or when run standalone from ToT.
 */
std::filesystem::path TestFilePath(const std::filesystem::path& rel_path);

/**
 * Returns the path to a bazel-generated test file, specified by a path relative to ToT.
 * Path is valid when run through bazel, or when run standalone from ToT.
 */
std::filesystem::path BazelBinTestFilePath(const std::filesystem::path& rel_path);

}  // namespace testing
}  // namespace px
