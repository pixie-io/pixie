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
#include <vector>

namespace pl {
namespace shared {
namespace stats {

class Tag {
 public:
  Tag(std::string_view name, std::string_view value) : name_(name), value_(value) {}

  bool operator==(const Tag& other) const { return other.name_ == name_ && other.value_ == value_; }

  const std::string& name() const { return name_; }

  const std::string& value() const { return value_; }

 private:
  std::string name_;
  std::string value_;
};

using TagVector = std::vector<Tag>;

}  // namespace stats
}  // namespace shared
}  // namespace pl
