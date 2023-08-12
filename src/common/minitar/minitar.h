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

#include <archive.h>

#include <filesystem>
#include <string>

#include "src/common/base/base.h"

namespace px {
namespace tools {

class Minitar {
 public:
  /**
   * Creates a new tar extractor for the given file.
   * @param tarball to extract.
   */
  explicit Minitar(std::filesystem::path file);

  ~Minitar();

  /**
   * Extract the files from the tarball initialized by the constructor.
   * @param dest_dir Directory in which to extract the tarball.
   * @param flags Controls the attributes of the files. Some possible values include:
   * ARCHIVE_EXTRACT_TIME, ARCHIVE_EXTRACT_PERM, ARCHIVE_EXTRACT_ACL, ARCHIVE_EXTRACT_FFLAGS.
   * Multiple flags can be set through the or oeprator.
   * See libarchive for flag defintitions and other possible flags.
   * @return error if the tarball could not be extracted.
   */
  Status Extract(std::string_view dest_dir = {}, std::string_view prefix_to_strip = {},
                 int flags = kDefaultFlags);

 private:
  static constexpr int kDefaultFlags = ARCHIVE_EXTRACT_TIME;
  struct archive* a;
  struct archive* ext;

  std::filesystem::path file_;
};

}  // namespace tools
}  // namespace px
