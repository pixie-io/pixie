#pragma once

#include <pypa/reader.hh>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"

namespace pl {
namespace carnot {
namespace compiler {

/**
 * StringReader implements libpypa's reader interface for a string.
 */
class StringReader : public pypa::Reader {
 public:
  explicit StringReader(std::string input) { lines_ = absl::StrSplit(input, '\n'); }

  ~StringReader() {}

  bool set_encoding(const std::string&) override { return true; }

  std::string next_line() override {
    std::string line;

    if (current_line_ < lines_.size()) {
      line = lines_[current_line_];
      current_line_++;
    }

    return line;
  }

  std::string get_line(size_t idx) override { return lines_[idx]; }

  unsigned get_line_number() const override { return current_line_; }

  std::string get_filename() const override { return ""; }

  bool eof() const override { return current_line_ >= lines_.size(); }

 private:
  size_t current_line_ = 0;
  std::vector<std::string> lines_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
