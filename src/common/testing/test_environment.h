#pragma once

#include <filesystem>
#include <string>

namespace pl {
namespace testing {

/**
 * Returns the path to the file, specified by a relative path, under the test base directory.
 */
std::filesystem::path TestFilePath(const std::filesystem::path& rel_path);

}  // namespace testing
}  // namespace pl
