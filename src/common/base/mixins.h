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

namespace px {
/**
 * Inheriting this class will make disallow automatic copies in
 * the subclass.
 */
class NotCopyable {
 public:
  NotCopyable(NotCopyable const&) = delete;
  NotCopyable& operator=(NotCopyable const&) = delete;
  NotCopyable() = default;
};

/**
 * Inheriting this class will make disallow automatic copies and moves in
 * the subclass.
 */
class NotCopyMoveable : public NotCopyable {
 public:
  NotCopyMoveable(NotCopyMoveable&&) = delete;
  NotCopyMoveable() = default;
};

}  // namespace px
