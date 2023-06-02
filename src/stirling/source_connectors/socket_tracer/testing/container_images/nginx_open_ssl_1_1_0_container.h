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

#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_worker_pid.h"

namespace px {
namespace stirling {
namespace testing {

class NginxOpenSSL_1_1_0_Container : public ContainerRunner {
 public:
  NginxOpenSSL_1_1_0_Container()
      : ContainerRunner(::px::testing::BazelRunfilePath(kBazelImageTar), kContainerNamePrefix,
                        kReadyMessage) {}

  int32_t NginxWorkerPID() const { return internal::GetNginxWorkerPID(process_pid()); }

 private:
  // Image is a modified nginx image created through bazel rules, and stored as a tar file.
  // It is not pushed to any repo.
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/"
      "nginx_openssl_1_1_0_image.tar";
  static constexpr std::string_view kContainerNamePrefix = "nginx";
  static constexpr std::string_view kReadyMessage = "";
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
