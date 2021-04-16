#pragma once

#include <filesystem>
#include <memory>

namespace px {
namespace fs {

/**
 * Returns a unique temporary file that is automatically be deleted.
 */
class TempFile {
 public:
  static std::unique_ptr<TempFile> Create() { return std::unique_ptr<TempFile>(new TempFile); }

  std::filesystem::path path() {
    // See https://en.cppreference.com/w/cpp/io/c/tmpfile.
    return std::filesystem::path("/proc/self/fd") / std::to_string(fileno(f_));
  }

  ~TempFile() { fclose(f_); }

 private:
  TempFile() { f_ = std::tmpfile(); }

  std::FILE* f_;
};

}  // namespace fs
}  // namespace px
