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

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"

using px::ContainerRunner;

class VaddrConvertSelfFuncContainer : public ContainerRunner {
 public:
  VaddrConvertSelfFuncContainer()
      : ContainerRunner(px::testing::BazelRunfilePath(kBazelImageTar), kInstanceNamePrefix,
                        kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/obj_tools/testdata/containers/vaddr_convert_self_func_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "vaddr_convert_self_func_container";
  static constexpr std::string_view kReadyMessage = "Running";
};
