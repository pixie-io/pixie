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

#include <absl/strings/str_replace.h>
#include <string>

namespace px {
namespace grpc {

/**
 * @brief Transform the method path from gRPC format to protobuf format.
 *
 * https://github.com/grpc/grpc/issues/19618 discussed upstreaming this to an official
 * repository, but probably won't happen in the foreseeable future.
 * Keeping this issue for reference.
 */
inline std::string MethodPath(std::string_view grpc_path) {
  while (grpc_path.front() == '/') {
    grpc_path.remove_prefix(1);
  }
  return absl::StrReplaceAll(grpc_path, {{"/", "."}});
}

}  // namespace grpc
}  // namespace px
