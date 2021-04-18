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
