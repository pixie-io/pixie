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
#include <string_view>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace websocket {

bool IsK8sChannelProtocol(std::string_view subprotocol);

std::string K8sExecCommand(std::string_view path);

bool K8sExecTTY(std::string_view path);

std::string CleanTerminalOutput(std::string_view output, bool tty);

std::string Printable(std::string_view data);

void DecodeK8sExec(std::string_view path, std::vector<Message> messages, ExecState* state,
                   std::vector<Record>* records);

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
