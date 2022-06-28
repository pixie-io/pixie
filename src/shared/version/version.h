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

namespace px {

class VersionInfo {
 public:
  /**
   * Return the revision ID.
   * @return string
   */
  static std::string Revision();

  /**
   * Get the repository status.
   * @return string
   */
  static std::string RevisionStatus();

  /**
   * Get the built by.
   * @return string
   */
  static std::string Builder();

  /**
   * Returns a version string with git info, release status.
   * @return string
   */
  static std::string VersionString();

  /**
   * Returns the build number if present (or 0).
   * @return int build number
   */
  static int BuildNumber();
};

}  // namespace px
