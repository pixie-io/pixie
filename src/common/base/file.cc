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

#include <sys/stat.h>

#include <fstream>
#include <string>

#include "src/common/base/error.h"
#include "src/common/base/file.h"
#include "src/common/base/logging.h"

namespace px {

std::string FileContentsOrDie(const std::string& filename) {
  return ReadFileToString(filename).ConsumeValueOrDie();
}

StatusOr<std::string> ReadFileToString(const std::string& filename, std::ios_base::openmode mode) {
  std::ifstream ifs(filename, mode);
  if (!ifs.good()) {
    return error::Internal("Failed to read file $0 ($1)", filename, strerror(errno));
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  return buffer.str();
}

Status WriteFileFromString(const std::string& filename, std::string_view contents,
                           std::ios_base::openmode mode) {
  std::ofstream ofs(filename, mode);
  if (!ofs.good()) {
    return error::Internal("Failed to write file $0", filename);
  }
  ofs << contents;
  return Status::OK();
}

}  // namespace px
