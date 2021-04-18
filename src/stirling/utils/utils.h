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

#include <string>

#include <absl/strings/substitute.h>

namespace px {
namespace stirling {

// TODO(oazizi): Move to socket_tracer. It's the only user.
struct NV {
  std::string name;
  std::string value;

  std::string ToString() const { return absl::Substitute("[name='$0' value='$1']", name, value); }
};

inline void RemoveRepeatingSuffix(std::string_view* str, char c) {
  size_t pos = str->find_last_not_of(c);
  if (pos != std::string_view::npos) {
    str->remove_suffix(str->size() - pos - 1);
  }
}

}  // namespace stirling
}  // namespace px
