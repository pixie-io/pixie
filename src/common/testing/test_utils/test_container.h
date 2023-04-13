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

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"

namespace px {

// TODO(oazizi): Remove all uses to this container in favor of SleepContainer below.
class EmailServiceContainer : public ContainerRunner {
 public:
  EmailServiceContainer()
      : ContainerRunner(px::testing::BazelRunfilePath(kImage), kInstanceNamePrefix, kReadyMessage) {
  }

 private:
  static constexpr std::string_view kImage = "src/common/testing/test_utils/emailservice_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "emailservice_container";
  static constexpr std::string_view kReadyMessage = "listening on port: 8080";
};

class SleepContainer : public ContainerRunner {
 public:
  SleepContainer()
      : ContainerRunner(px::testing::BazelRunfilePath(kBazelImageTar), kInstanceNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/common/testing/test_utils/sleep_container_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "sleep_container";
  static constexpr std::string_view kReadyMessage = "started";
};

}  // namespace px
