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
namespace stirling {

// Provides a string view into a char array included in the binary via objcopy.
// Useful for include BPF programs that are copied into the binary.
#define OBJ_STRVIEW(varname, build_label)              \
  extern char _binary_##build_label##_start;           \
  extern char _binary_##build_label##_end;             \
  inline const std::string_view varname =              \
      std::string_view(&_binary_##build_label##_start, \
                       &_binary_##build_label##_end - &_binary_##build_label##_start);

// Macro to load BPF source code embedded in object files.
// See 'pl_bpf_cc_resource' bazel rule to see how these are generated.
// TODO(yzhao): Might replace users of this with OBJ_STRVIEW.
#define BPF_SRC_STRVIEW(varname, build_label) OBJ_STRVIEW(varname, build_label);

// Define NO_OPT_ATTR that specifies that function should not be optimized away.
// Typically used on functions used purely as eBPF probe triggers.
// Note that the attributes are different depending on the compiler.
#if defined(__clang__)
#define NO_OPT_ATTR __attribute__((noinline, optnone))
#elif defined(__GNUC__) || defined(__GNUG__)
#define NO_OPT_ATTR __attribute__((noinline, optimize("O0")))
#endif

}  // namespace stirling
}  // namespace px
