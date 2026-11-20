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

#include "src/stirling/source_connectors/socket_tracer/protocols/http/credentials.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http {

std::string CredentialFingerprint(std::string_view) { return ""; }

std::optional<std::string> RedactCredentialHeader(std::string_view, std::string_view) {
  return std::nullopt;
}

std::string RedactCredentialPath(std::string_view path) { return std::string(path); }

std::string RedactCredentialBody(std::string_view body) { return std::string(body); }

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
