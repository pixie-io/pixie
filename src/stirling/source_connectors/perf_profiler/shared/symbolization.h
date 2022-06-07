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
#include <string_view>

namespace px {
namespace stirling {
namespace symbolization {
// The separator between symbols in the folded stack trace string.
// e.g. main;foo;bar;printf
constexpr std::string_view kSeparator = ";";

// The prefix is attached to each symbol in a folded stack trace string.
constexpr std::string_view kUserPrefix = "";
constexpr std::string_view kKernelPrefix = "[k] ";
constexpr std::string_view kJavaPrefix = "[j] ";

// This is the symbol we see for Java interpreter frames. The stringifier
// will collapse repeated instances of this into something like "[j] Interpreter [12x]".
constexpr std::string_view kJavaInterpreter = "[j] Interpreter";

// The drop message indicates that the kernel had a hash table collision
// and dropped tracking of one stack trace.
constexpr std::string_view kDropMessage = "<stack trace lost>";

// The option added to java commands to preserve frame pointer.
constexpr std::string_view kJavaPreserveFramePointerOption = "-XX:+PreserveFramePointer";
}  // namespace symbolization
}  // namespace stirling
}  // namespace px
