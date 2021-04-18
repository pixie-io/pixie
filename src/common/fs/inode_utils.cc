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

#include "src/common/fs/inode_utils.h"

namespace px {
namespace fs {

// Extract the inode number from a string that looks like the following: "socket:[32431]"
StatusOr<uint32_t> ExtractInodeNum(std::string_view inode_type_prefix, std::string_view link_str) {
  if (!absl::StartsWith(link_str, inode_type_prefix)) {
    return error::Internal("FD does not appear to be a valid socket inode string. FD link = $0",
                           link_str);
  }

  link_str.remove_prefix(inode_type_prefix.size());
  if (link_str.empty() || link_str.front() != '[' || link_str.back() != ']') {
    return error::Internal("Malformed inode string.");
  }
  link_str.remove_prefix(1);
  link_str.remove_suffix(1);

  uint32_t inode_num;
  if (!absl::SimpleAtoi(link_str, &inode_num)) {
    return error::Internal("Could not parse inode string.");
  }

  return inode_num;
}

}  // namespace fs
}  // namespace px
