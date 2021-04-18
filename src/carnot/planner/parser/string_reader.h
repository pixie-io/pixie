/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>
#include <pypa/reader.hh>

#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * StringReader implements libpypa's reader interface for a string.
 */
class StringReader : public pypa::Reader {
 public:
  explicit StringReader(std::string_view input) { lines_ = absl::StrSplit(input, '\n'); }

  ~StringReader() override = default;

  bool set_encoding(const std::string&) override { return true; }

  std::string next_line() override {
    std::string line;

    if (current_line_ < lines_.size()) {
      line = lines_[current_line_];
      current_line_++;
    }

    if (current_line_ == lines_.size()) {
      return line;
    }
    // Libpypa expects a newline after every line not at the eof.
    return line + "\n";
  }

  std::string get_line(size_t idx) override {
    // Libpypa calls get_line() with 1 indexed values, this maps to the correct value.
    return lines_[idx - 1];
  }

  unsigned get_line_number() const override {
    // This is to make sure that the line number returned is 1-indexed and within the lines vector.
    auto value = std::min(std::max<size_t>(1, current_line_), lines_.size());
    return value;
  }

  std::string get_filename() const override { return ""; }

  bool eof() const override { return current_line_ >= lines_.size(); }

 private:
  size_t current_line_ = 0;
  std::vector<std::string> lines_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
