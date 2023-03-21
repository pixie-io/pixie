#!/bin/bash -e

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

# This script only exists because stirling_wrapper must be run as root.
# Otherwise one would use bazel run ...

script_dir=$(dirname "$0")

# shellcheck source=./src/stirling/scripts/utils.sh
source "$script_dir"/utils.sh

# Pass in flags like `-c opt` here if you like.
flags=""

bazel build $flags //src/stirling/binaries:stirling_wrapper

cmd=$(bazel cquery $flags //src/stirling/binaries:stirling_wrapper --output starlark --starlark:expr "target.files.to_list()[0].path" 2> /dev/null)
run_prompt_sudo "$cmd" "$@"
